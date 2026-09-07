#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

#include <httplib/httplib.h>

#include "Logger.h"
#include "UploadPaths.h"

/// 上传文件服务（读取侧）路由注册：GET /uploads/<subdir>/<filename>。
///
/// 设计说明：
/// · 为什么不用 httplib::set_mount_point：该方案无法按本项目的
///   GOCOOK_UPLOADS_DIR（common/UploadPaths.h）与双层穿越防御定制，可控性差。
/// · URL 前缀与磁盘目录都由 subdir 决定（/uploads/<subdir>/<file> ↔
///   <上传根>/<subdir>/<file>），与 UploadPaths::urlToPath 的约定天然一致，
///   新增业务子目录只需一行注册。
/// · 统一语义：非法文件名（含 .. 或 /）→ 400 + 审计日志；
///   文件不存在 → 404；存在但打不开（权限/竞态）→ 500；其余异常 → 500。
/// · 扩展名统一小写化后匹配 MIME，避免 ".PNG" 等大写扩展名被误判为默认 jpeg。
/// · 全部响应携带 CSP sandbox + nosniff：svg（含历史存量文件与写侧名单漏网内容）
///   以顶级文档打开时无脚本/iframe/同源能力；作为 <img> 或由 Qt 客户端加载时
///   浏览器不执行 CSP，不受影响。权威防线在读取侧，名单局限见
///   ImageUploadRules.h 的 svg 名单注释。
///
/// 注：本表保留 image/webp 仅为兼容历史文件；写侧为何禁止上传 webp 见
/// common/ImageUploadRules.h 的白名单注释。
namespace UploadFileServer {

/// 日志安全化：换行/回车控制字符替换为可见转义。
/// 文件名来自 URL 百分号解码（httplib 在路由前解码），可携带 %0A/%0D，
/// 直接拼进日志可被用来伪造审计日志行（log forging）。
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
/// @param label  日志文案用的中文名称（如"头像"），为空时回退用 subdir
inline void registerUploadRoutes(httplib::Server& svr,
                                 const std::string& subdir,
                                 const std::string& label = {}) {
    const std::string dir = UploadPaths::baseDir() + "/" + subdir + "/";
    const std::string display = label.empty() ? subdir : label;

    try {
        std::filesystem::create_directories(dir);
    } catch (const std::exception& e) {
        LOG_WARN("无法创建%s上传目录 %s: %s", display.c_str(), dir.c_str(), e.what());
    }
    LOG_INFO("%s存储目录：%s", display.c_str(), dir.c_str());

    svr.Get("/uploads/" + subdir + "/(.+)",
            [dir, display](const httplib::Request& req, httplib::Response& res) {
        try {
            // svg 等以顶级文档打开时可执行脚本（写侧黑名单不承诺完备，历史存量
            // 文件亦未过滤），统一加 CSP sandbox 兜底：文档无脚本/iframe/同源访问
            // 能力；作为 <img> 或 Qt 客户端加载时浏览器不执行 CSP，不受影响。
            // nosniff 防止浏览器对响应做内容嗅探。
            res.set_header("Content-Security-Policy",
                           "sandbox; default-src 'none'; style-src 'unsafe-inline'");
            res.set_header("X-Content-Type-Options", "nosniff");

            const std::string filename = req.matches[1];

            // 第一层防御：拦截基本路径穿越尝试（.. 和 /）
            if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
                res.status = 400;
                res.set_content("请求错误", "text/plain");
                return;
            }
            // 第二层防御：weakly_canonical 验证最终路径（含符号链接解析）仍在允许目录内
            const std::string filePath = dir + filename;
            const std::filesystem::path normPath = std::filesystem::weakly_canonical(filePath);
            const std::filesystem::path allowedDir = std::filesystem::weakly_canonical(dir);
            const std::string normStr = normPath.string();
            const std::string allowStr = allowedDir.string();
            // 前缀命中后还需路径组件边界：路径相等，或下一字符必须是 '/'。
            // 否则目录内符号链接指向 ".../avatars_xxx/..." 这类与本目录同前缀的
            // 兄弟目录时，纯字符串前缀判断会误放行（泄露目录外文件）
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
            // 不存在或非常规文件（目录等）→ 404（先判存在，把"打不开"与"不存在"
            // 区分开）：ifstream 打开目录会成功却读到空内容（200 空 body），FIFO 等
            // 特殊文件会阻塞读取；符号链接解析到普通文件时 is_regular_file 为 true
            if (!std::filesystem::exists(normStr)
                || !std::filesystem::is_regular_file(normStr)) {
                res.status = 404;
                res.set_content("未找到", "text/plain");
                return;
            }
            std::ifstream ifs(normStr, std::ios::binary);
            if (!ifs) {
                LOG_ERROR("无法打开%s文件：%s", display.c_str(), normStr.c_str());
                res.status = 500;
                res.set_content("服务器内部错误，请稍后重试", "text/plain");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            // 扩展名小写化后匹配 MIME（默认按 jpeg 处理）
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
            else if (ext == ".webp") mime = "image/webp";  // 历史文件兼容（写侧禁传，见 ImageUploadRules.h）
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
