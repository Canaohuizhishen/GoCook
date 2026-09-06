#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

#include <httplib/httplib.h>

#include "Logger.h"
#include "UploadPaths.h"

// ============================================================================
// 文件读取侧静态文件服务
// ============================================================================
// 职责：注册 GET /uploads/<subdir>/<filename> 路由，将 URL 映射到磁盘文件。
//
// 核心安全承诺：
//   1. 路径防御：双层校验（基础拦截 + weakly_canonical 边界锁定），防目录穿越。
//   2. 响应加固：全量携带 CSP sandbox 与 nosniff，作为 SVG 等可执行文件的终极兜底防线。
//   3. 规则收口：目录解析统一走 UploadPaths，支持 GOCOOK_UPLOADS_DIR 环境变量。
//
// 相较于 httplib::set_mount_point，本实现支持自定义环境变量与审计日志，可控性更强。
// ============================================================================
namespace UploadFileServer {

/// 日志安全化：将文件名中的 \r \n 转义为可见字符。
/// 原因：URL 解码后可能携带 %0A/%0D，直接拼入日志会伪造日志行（Log Forging）。
inline std::string escapeLogText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\r')      out += "\\r";
        else if (c == '\n') out += "\\n";
        else                 out += c;
    }
    return out;
}

/// 注册 /uploads/<subdir>/<filename> 的 GET 静态文件服务。
/// @param svr    目标 httplib 服务器
/// @param subdir 业务子目录（"avatars"[头像]、"recipes"[菜谱封面/步骤图]），
///               同时决定 URL 前缀与磁盘子目录
/// @param label  日志文案用的中文名称（如"头像"），便于运维排查，为空时回退用 subdir
inline void registerUploadRoutes(httplib::Server& svr,
                                 const std::string& subdir,
                                 const std::string& label = {}) {
    const std::string dir = UploadPaths::baseDir() + "/" + subdir + "/";
    const std::string display = label.empty() ? subdir : label;

    // 启动时尝试创建目录（失败仅警告，不影响服务启动，请求时会返回 404）
    try {
        std::filesystem::create_directories(dir);
    } catch (const std::exception& e) {
        LOG_WARN("无法创建%s上传目录 %s: %s", display.c_str(), dir.c_str(), e.what());
    }
    LOG_INFO("%s存储目录：%s", display.c_str(), dir.c_str());

    svr.Get("/uploads/" + subdir + "/(.+)",
            [dir, display](const httplib::Request& req, httplib::Response& res) {
        try {
            // 1. 安全响应头（无论成功或失败都应携带）
            // CSP sandbox：禁止脚本/iframe/同源访问，用于兜底 SVG 等可执行文件的 XSS 风险。
            // nosniff：防止浏览器嗅探 MIME 类型。
            res.set_header("Content-Security-Policy",
                           "sandbox; default-src 'none'; style-src 'unsafe-inline'");
            res.set_header("X-Content-Type-Options", "nosniff");

            const std::string filename = req.matches[1];

            // 2. 第一层防御：快速拦截明显的路径穿越字符
            if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
                res.status = 400;
                res.set_content("请求错误", "text/plain");
                return;
            }
            // 3. 第二层防御：weakly_canonical 验证最终路径（含符号链接解析）仍在允许目录内
            const std::string filePath = dir + filename;
            const std::filesystem::path normPath = std::filesystem::weakly_canonical(filePath);
            const std::filesystem::path allowedDir = std::filesystem::weakly_canonical(dir);
            const std::string normStr = normPath.string();
            const std::string allowStr = allowedDir.string();

            // 关键点：不仅要前缀匹配，还必须检查下一个字符是 '/' 或路径相等。
            // 防止放行 "/uploads/avatars_evil" 这类同前缀的兄弟目录（符号链接攻击）。
            const bool inside = normStr.rfind(allowStr, 0) == 0
                && (normStr.size() == allowStr.size() || normStr[allowStr.size()] == '/');
            if (!inside) {
                LOG_WARN("已拦截%s路径穿越请求：%s → %s",
                         display.c_str(),
                         escapeLogText(filename).c_str(),
                         escapeLogText(normStr).c_str());
                res.status = 400;
                res.set_content("请求错误", "text/plain");
                return;
            }

            // 4. 文件存在性与类型校验
            // 只服务常规文件（is_regular_file）。
            // 规避 ifstream 陷阱：读目录返回空（误报 200），读管道会挂起线程（DoS）。
            if (!std::filesystem::exists(normStr)
                || !std::filesystem::is_regular_file(normStr)) {
                res.status = 404;
                res.set_content("未找到", "text/plain");
                return;
            }

            // 文件存在但无法打开（如权限问题、竞态删除） -> 500（区别于 404）
            std::ifstream ifs(normStr, std::ios::binary);
            if (!ifs) {
                LOG_ERROR("无法打开%s文件：%s", display.c_str(), normStr.c_str());
                res.status = 500;
                res.set_content("服务器内部错误，请稍后重试", "text/plain");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            // 5. MIME 类型匹配（扩展名小写化）
            // 解决 ".PNG" 被误判为 image/jpeg 的问题。
            auto dotPos = filename.find_last_of('.');
            std::string ext;
            if (dotPos != std::string::npos) {
                for (char c : filename.substr(dotPos))
                    ext += std::tolower(static_cast<unsigned char>(c));
            }
            std::string mime = "image/jpeg";
            if (ext == ".png")       mime = "image/png";
            else if (ext == ".gif")  mime = "image/gif";
            else if (ext == ".bmp")  mime = "image/bmp";
            else if (ext == ".svg")  mime = "image/svg+xml";

            res.set_content(content, mime);
        } catch (const std::exception& e) {
            LOG_ERROR("提供%s文件时出错：%s", display.c_str(), e.what());
            res.status = 500;
            res.set_content("服务器内部错误，请稍后重试", "text/plain");
        }
    });
}

} // namespace UploadFileServer
