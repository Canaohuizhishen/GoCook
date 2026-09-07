#pragma once

#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>

/// 图片上传规则收口（写入侧）。
///
/// 三个上传入口（头像 / 菜谱封面 / 菜谱步骤图）共用本模块的全部规则：
/// · 白名单、Content-Type → 扩展名映射、5MB 上限、Content-Type 头解析、
///   内容魔数嗅探（宽容修正）、临时文件写盘。
/// 规则集中单点，避免"复制三份、改一处漏两处"的规则漂移。
///
/// 读取侧（对外服务已存文件）见 common/UploadFileServer.h：
/// 它按"文件扩展名"定 MIME 且保留 webp —— 那是为兼容历史文件，与写侧禁
/// webp 并不矛盾（见下方白名单注释）。
namespace ImageUploadRules {

/// 图片上传大小上限（5MB，与客户端一致）
inline constexpr size_t kMaxImageBytes = 5 * 1024 * 1024;

/// 从 Content-Type 头提取 MIME 类型：去掉 ";boundary=..." 等参数、trim 首尾空白，
/// 并统一转小写（MIME 类型按 RFC 2045 大小写不敏感，避免 "Image/PNG" 这类写法
/// 被白名单误拒）
inline std::string extractMimeType(const std::string& ct) {
    auto p = ct.find(';');
    std::string m = (p == std::string::npos) ? ct : ct.substr(0, p);
    while (!m.empty() && (m.front() == ' ' || m.front() == '\t')) m.erase(0, 1);
    while (!m.empty() && (m.back() == ' ' || m.back() == '\t')) m.pop_back();
    for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return m;
}

/// 允许上传的图片类型白名单。
/// 注意：故意不含 image/webp —— Qt 客户端无法渲染 WebP（上传对话框与
/// QML Image 显示均不支持）。读侧 MIME 表保留 webp 仅为兼容历史文件，
/// 如需放开请同时评估客户端渲染能力，切勿单边修改。
inline bool isSupportedImageType(const std::string& mime) {
    return mime == "image/jpeg" || mime == "image/jpg" || mime == "image/png"
        || mime == "image/gif" || mime == "image/bmp" || mime == "image/svg+xml";
}

/// 按 MIME 类型选择落盘扩展名（未知类型回退 .jpg，与历史默认一致）
inline std::string imageTypeToExtension(const std::string& mime) {
    if (mime == "image/png")      return ".png";
    if (mime == "image/gif")      return ".gif";
    if (mime == "image/bmp")      return ".bmp";
    if (mime == "image/svg+xml")  return ".svg";
    return ".jpg";
}

/// 忽略大小写的子串查找（用于 svg 的 <script> 检查）
inline bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        size_t j = 0;
        for (; j < needle.size(); ++j) {
            if (std::tolower(static_cast<unsigned char>(haystack[i + j]))
                != std::tolower(static_cast<unsigned char>(needle[j])))
                break;
        }
        if (j == needle.size()) return true;
    }
    return false;
}

/// 魔数嗅探内容真实图片类型；无法识别返回空串。
/// · jpeg: FF D8 FF   png: 89 50 4E 47 0D 0A 1A 0A   gif: GIF87a/GIF89a   bmp: BM
/// · svg 是文本格式无固定魔数：启发式 = 出现 <svg 标签，且内容不得命中
///   恶意名单 kSvgForbiddenSubstrings（svg 的可执行内容不止 <script）。
///   名单见下方实现处注释。
/// · svgz（gzip 压缩 svg）等无法识别 → 返回空串，由上层按"内容与声明不符"
///   拒绝（历史实现会把 gzip 字节存成 .svg，读侧按 svg MIME 下发 = 坏图）。
inline std::string sniffImageType(const std::string& content) {
    const size_t n = content.size();
    if (n >= 3
        && static_cast<unsigned char>(content[0]) == 0xFF
        && static_cast<unsigned char>(content[1]) == 0xD8
        && static_cast<unsigned char>(content[2]) == 0xFF)
        return "image/jpeg";
    static constexpr unsigned char kPng[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n >= 8 && std::memcmp(content.data(), kPng, 8) == 0) return "image/png";
    if (n >= 6 && (content.compare(0, 6, "GIF87a") == 0 || content.compare(0, 6, "GIF89a") == 0))
        return "image/gif";
    if (n >= 2 && content.compare(0, 2, "BM") == 0) return "image/bmp";
    // svg 恶意内容第一道防线（尽力而为的名单，命中任一即拒）：
    // · <script 标签（大小写变体由 containsIgnoreCase 覆盖）、javascript: URI
    //   （href 注入）；
    // · 常见 on* 事件处理器属性：浏览器以顶级文档打开 svg 时会执行（stored-XSS
    //   载体），Qt 渲染 svg 不执行脚本，风险面主要在浏览器直接访问 URL。
    // 本名单不承诺完备，已知盲区：
    // · 只覆盖列出的属性名——foreignObject 是 HTML 集成点，其内嵌内容按 HTML
    //   解析，iframe srcdoc / data: URI 等把脚本经实体或 URL 编码后，原始字节
    //   不含名单字面量即可绕过（"XML 属性名必须为字面量"仅对属性名成立，不
    //   适用于属性值与 HTML 内容）；
    // · 未列出的自动触发事件属性（onanimationstart、媒体事件族等）同样能执行。
    // 权威防线在读取侧：UploadFileServer 对全部响应下发 CSP sandbox（见
    // common/UploadFileServer.h），顶级打开时脚本/iframe 一律被禁——历史存量
    // svg 与名单漏网内容同样不可执行。宁可误杀——文本/注释恰好含名单词也会被
    // 拒（合法上传可重选，恶意内容放行不可逆）。
    static constexpr const char* kSvgForbiddenSubstrings[] = {
        "<script",
        "onload", "onerror", "onclick",
        "onmousedown", "onmouseup", "onmouseover", "onmousemove", "onmouseout",
        "onfocus", "onblur",
        "onkeydown", "onkeypress", "onkeyup",
        "onbegin", "onend", "onrepeat",
        "javascript:",
    };
    if (content.find("<svg") != std::string::npos) {
        for (const char* bad : kSvgForbiddenSubstrings) {
            if (containsIgnoreCase(content, bad)) return {};
        }
        return "image/svg+xml";
    }
    return {};
}

/// 上传校验与落盘扩展名决策的结果
enum class UploadError { None, UnsupportedType, TooLarge, ContentMismatch };

/// 校验一次图片上传并输出落盘扩展名（含前导 '.'），检查顺序：
/// 白名单 → 5MB 上限 → 内容嗅探（保证超限内容不被全量子串扫描）。
/// 1) 声明 Content-Type 必须过白名单，否则 UnsupportedType（调用方保留各自
///    的"不支持的图片格式"文案，包括头像侧对 WebP 的特别说明）。
/// 2) 内容超过 kMaxImageBytes → TooLarge（调用方返回各自"图片大小不能超过
///    5MB"文案；嗅探成本与内容大小成正比，须先于嗅探拒绝）。
/// 3) 魔数嗅探实际类型，采用"宽容修正"而非硬拒：
///    · 实际类型可识别 → 按真实类型落盘。客户端按文件扩展名猜 Content-Type
///      （HttpGoCookApi.cpp），声明常与实际失真（如 .jpg 实为 png），宽容修正
///      避免误杀合法上传，顺带修复存错后缀的坏图。
///    · 内容不可识别（伪装格式 / svg 含脚本 / svgz 等）→ ContentMismatch，
///      调用方返回统一的 400 新文案。
inline UploadError classifyUpload(const std::string& content,
                                  const std::string& declaredMime,
                                  std::string& outExtension) {
    if (!isSupportedImageType(declaredMime))
        return UploadError::UnsupportedType;
    if (content.size() > kMaxImageBytes)
        return UploadError::TooLarge;
    const std::string actualMime = sniffImageType(content);
    if (actualMime.empty())
        return UploadError::ContentMismatch;
    outExtension = imageTypeToExtension(actualMime);
    return UploadError::None;
}

/// 把上传内容写入 /tmp 下唯一命名的临时文件，返回完整路径；
/// 打开失败返回 nullopt（调用方自行报 500"文件写入失败"）。
/// @param tag 唯一性前缀（如 "avatar_1"、"recipe_24_step_0"），
///            实际命名 /tmp/gocook_<tag>_<时间戳><ext>，与历史格式一致
inline std::optional<std::string> writeTempImageFile(const std::string& tag,
                                                     const std::string& content,
                                                     const std::string& ext) {
    const std::string tempPath = "/tmp/gocook_" + tag + "_"
        + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ext;
    std::ofstream ofs(tempPath, std::ios::binary);
    if (!ofs)
        return std::nullopt;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.close();
    return tempPath;
}

} // namespace ImageUploadRules
