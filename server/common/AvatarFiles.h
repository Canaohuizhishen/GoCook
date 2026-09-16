#pragma once

#include <filesystem>
#include <string>

#include "Logger.h"
#include "UploadPaths.h"

// ============================================================================
// 头像文件生命周期原语（上传暂存 / 保存绑定 / 回收清理共用）
// ============================================================================
// 职责：头像文件从"暂存"到"绑定"到"回收"全程的归属判定、存在性校验与删除，
// 是全链路（上传写侧、绑定校验、换绑回收、丢弃暂存、注销清理、后台 GC）的唯一实现点。
//
// 核心约定：
//   1. 命名空间：服务端管理的头像 URL 恒为 "/uploads/avatars/user_<userId>_<时间戳>.<ext>"
//      （生成方见 PgUserRepository::uploadAvatar），前缀 + 用户 ID 双段校验，
//      拒绝"路径穿越 / 他人文件 / 同名前缀（user_1_ vs user_10_）"三类越权。
//   2. 删除永不抛出：回收失败仅告警，残留文件由后台 GC 兜底
//      （文件删除不参与业务事务，沿用 PgRecipeRepository 的既成约定）。
//   3. 幂等：文件已不存在视为成功——丢弃暂存等操作可安全重试。
// ============================================================================
namespace AvatarFiles {

/// 托管头像 URL 前缀（服务端管理的头像命名空间）
inline constexpr const char* kUrlPrefix = "/uploads/avatars/";

/// 判断 URL 是否落在托管头像命名空间（不限用户）。
/// 仅接受 "/uploads/avatars/<单一文件名>" 形态：文件名不允许再含 '/'（挡路径穿越/子目录）。
inline bool isManagedAvatarUrl(const std::string& url) {
    const std::string prefix = kUrlPrefix;
    if (url.rfind(prefix, 0) != 0) return false;
    const std::string filename = url.substr(prefix.size());
    return !filename.empty() && filename.find('/') == std::string::npos;
}

/// 判断 URL 是否为指定用户的托管头像。
/// 文件名须以 "user_<userId>_" 开头且严格边界匹配（"user_1_" 不会误配 "user_10_..."）。
inline bool isManagedAvatarUrl(const std::string& url, int userId) {
    if (!isManagedAvatarUrl(url)) return false;
    const std::string expected = "user_" + std::to_string(userId) + "_";
    const std::string filename = url.substr(std::string(kUrlPrefix).size());
    return filename.rfind(expected, 0) == 0 && filename.size() > expected.size();
}

/// 托管头像文件是否存在于磁盘（仅常规文件；目录/特殊文件一律视为不存在）。
inline bool fileExists(const std::string& url) {
    if (!isManagedAvatarUrl(url)) return false;
    const std::string path = UploadPaths::urlToPath(url, "avatars");
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

/// 删除托管头像文件（幂等：文件已不存在视为成功）。
/// 仅做命名空间与文件类型防御；归属校验由调用方在删除前完成（如"是否为本人暂存"）。
/// 失败仅告警，不抛出——回收永远不应让业务请求失败，残留由 GC 兜底。
inline bool removeFile(const std::string& url) {
    if (!isManagedAvatarUrl(url)) {
        LOG_WARN("拒绝回收非托管头像路径：%s", url.c_str());
        return false;
    }
    const std::string path = UploadPaths::urlToPath(url, "avatars");
    if (path.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;  // 幂等：已不存在
    if (!std::filesystem::is_regular_file(path, ec)) {
        LOG_WARN("拒绝回收非常规文件（目录/特殊文件不动）：%s", path.c_str());
        return false;
    }
    std::filesystem::remove(path, ec);
    if (ec) {
        LOG_WARN("回收头像文件失败（后台 GC 将在其后兜底）：%s - %s",
                 path.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

} // namespace AvatarFiles
