#pragma once

#include <string>
#include <filesystem>
#include <cstdlib>

/// 上传文件路径规则统一收口：
/// 存储 URL（/uploads/<subdir>/<filename>）↔ 磁盘真实路径（<上传根目录>/<subdir>/<filename>）。
/// 上传根目录：GOCOOK_UPLOADS_DIR 环境变量优先，默认 "server/uploads"（转为绝对路径）。
/// 写入方（updateRecipeImage / updateStepImage / uploadAvatar）与读取方（Router 文件服务）
/// 共用本规则，避免 URL 约定散落多处、改一处漏一处。

namespace UploadPaths {

/// 上传根目录的绝对路径（无尾部斜杠）；环境变量未设置时回退 "server/uploads"
inline std::string baseDir() {
    const char* envDir = std::getenv("GOCOOK_UPLOADS_DIR");
    return std::filesystem::absolute(envDir ? envDir : "server/uploads").string();
}

/// 存储 URL → 磁盘真实路径；URL 不含 '/'（取不到文件名）时返回空串，调用方应跳过。
/// subdir（业务类别："avatars"[用户头像]、"recipes"[菜谱封面图与步骤图]，
/// 步骤图与封面同存 recipes，无独立 steps 子目录）
inline std::string urlToPath(const std::string& url, const std::string& subdir) {
    auto pos = url.find_last_of('/');
    if (pos == std::string::npos) return {};
    return baseDir() + "/" + subdir + "/" + url.substr(pos + 1);
}

} // namespace UploadPaths
