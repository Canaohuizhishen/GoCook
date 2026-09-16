#pragma once

#include <chrono>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "ConnectionPool.h"
#include "DbExecutor.h"
#include "Logger.h"
#include "UploadPaths.h"

// ============================================================================
// 上传孤儿文件 GC（一期：头像）
// ============================================================================
// 职责：回收"超过宽限期仍未被任何用户绑定"的头像文件，兜底一切未被即时
// 回收路径（换绑 / 丢弃 / 注销）覆盖的残留：
//   · 上传成功但客户端未保存、未丢弃就退出（暂存孤儿）
//   · 即时回收失败（权限/竞态）的残留
//   · 历史遗留的存量孤儿（升级后首次启动即被清扫）
//
// 核心设计：
//   1. 两阶段 + 双快照：先收集候选，再重新拉取引用集合后才删除——
//      把"收集"与"删除"之间发生的绑定（保存）从删除名单中排除，缩小竞态窗口。
//   2. 宽限期（调用方传入，默认 24h）：mtime 晚于 now-minAge 的文件一律保留，
//      保护"上传 → 保存"慢路径与进行中的操作。
//   3. 保守删除：仅常规文件；目录 / 符号链接 / 特殊文件一律跳过——
//      清理器绝不穿透链接（防误删链接目标），也绝不删除目录。
//   4. 组件不捕获异常：DB 故障由调用方（main 后台任务）捕获并告警，下轮重试。
// ============================================================================
namespace UploadOrphanGc {

/// 单轮清扫统计（scanned = 目录条目总数；kept 含"二次核对救回"的候选）
struct GcResult {
    size_t scanned = 0;  ///< 目录条目总数
    size_t skipped = 0;  ///< 非普通文件（目录 / 符号链接 / 特殊文件）
    size_t kept    = 0;  ///< 保留（被引用 / 未过宽限期 / 二次核对救回）
    size_t deleted = 0;  ///< 已回收
    size_t failed  = 0;  ///< 删除失败（下轮重试）
};

/// 阶段一（纯函数，可单测）：收集候选 = 常规文件 && 文件名不在 referenced
/// && mtime 早于 now-minAge。不执行任何删除；非普通文件计入 skipped。
inline std::vector<std::filesystem::path> collectCandidates(
    const std::string& dir,
    const std::set<std::string>& referenced,
    std::chrono::seconds minAge,
    GcResult& stats) {
    std::vector<std::filesystem::path> candidates;
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) {
        LOG_WARN("GC 扫描目录不可用，跳过本轮：%s - %s", dir.c_str(), ec.message().c_str());
        return candidates;
    }
    const auto now = std::filesystem::file_time_type::clock::now();
    for (const auto& entry : it) {
        ++stats.scanned;
        std::error_code fec;
        const auto status = entry.symlink_status(fec);
        if (fec || status.type() != std::filesystem::file_type::regular) {
            ++stats.skipped;  // 目录/符号链接/特殊文件一律不动
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (referenced.count(filename)) {
            ++stats.kept;
            continue;
        }
        const auto mtime = entry.last_write_time(fec);
        if (fec) {
            ++stats.failed;
            continue;
        }
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(now - mtime);
        if (age < minAge) {
            ++stats.kept;  // 宽限期内（保护"上传→保存"慢路径）
            continue;
        }
        candidates.push_back(entry.path());
    }
    return candidates;
}

/// 阶段二（纯函数，可单测）：删除候选中仍不在 referencedAfter 里的文件。
/// 在二次快照中新变为"被引用"的候选计为 kept（二次核对救回），不删除。
inline void removeCandidates(const std::vector<std::filesystem::path>& candidates,
                             const std::set<std::string>& referencedAfter,
                             GcResult& stats) {
    for (const auto& path : candidates) {
        const std::string filename = path.filename().string();
        if (referencedAfter.count(filename)) {
            ++stats.kept;  // 收集与删除之间被绑定（保存）——救回
            continue;
        }
        std::error_code ec;
        const bool removed = std::filesystem::remove(path, ec);
        if (ec) {
            ++stats.failed;
            LOG_WARN("GC 回收失败（下轮重试）：%s - %s", path.c_str(), ec.message().c_str());
        } else {
            // remove 返回 false 且无错误 = 文件已不在（并发删除），同视为已回收
            (void)removed;
            ++stats.deleted;
            LOG_INFO("GC 已回收孤儿头像文件：%s", filename.c_str());
        }
    }
}

/// 收集 users.avatar_url 中托管的头像文件名（仅 /uploads/avatars/ 前缀；
/// http(s) 外部链接不参与引用判定）。
inline std::set<std::string> referencedAvatarFilenames(ConnectionPool& db) {
    return executeDb(db, [](pqxx::work& txn) -> std::set<std::string> {
        LOG_DEBUG("[SQL] SELECT avatar_url FROM users WHERE avatar_url LIKE '/uploads/avatars/%'");
        pqxx::result rows = txn.exec(
            "SELECT avatar_url FROM users WHERE avatar_url LIKE '/uploads/avatars/%'");
        std::set<std::string> names;
        for (const auto& row : rows) {
            const std::string url = row["avatar_url"].as<std::string>("");
            const auto pos = url.find_last_of('/');
            if (pos != std::string::npos && pos + 1 < url.size())
                names.insert(url.substr(pos + 1));
        }
        return names;
    }, "数据库操作失败");
}

/// 组装（供 main 后台任务调用）：双快照 + 两阶段清扫，返回统计。
inline GcResult sweepAvatars(ConnectionPool& db, std::chrono::seconds minAge) {
    GcResult stats;
    const std::string dir = UploadPaths::baseDir() + "/avatars";
    const auto refsBefore = referencedAvatarFilenames(db);
    auto candidates = collectCandidates(dir, refsBefore, minAge, stats);
    const auto refsAfter = referencedAvatarFilenames(db);
    removeCandidates(candidates, refsAfter, stats);
    return stats;
}

} // namespace UploadOrphanGc
