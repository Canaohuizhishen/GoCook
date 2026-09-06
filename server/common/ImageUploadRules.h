#pragma once

#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>

// ============================================================================
// 图片上传写侧规则收口
// ============================================================================
// 职责：为头像、菜谱封面、步骤图三个上传入口，提供统一的校验与临时落盘能力。
//
// 核心设计：
//   1. 规则收敛：白名单 / 5MB 上限 / Content-Type 解析 / 魔数嗅探集中于此，
//      消除三处复制导致的规则漂移。
//   2. 校验顺序：白名单 -> 5MB 上限 -> 内容嗅探（超限内容不做全量扫描，节约 CPU）。
//   3. 宽容修正：魔数嗅探按真实类型落盘。客户端常按扩展名猜 Content-Type
//      （如 .jpg 实为 png），宽容修正可避免误杀合法上传，顺带修复坏图后缀。
//   4. 安全边界：SVG 脚本拦截为尽力而为（黑名单），盲区由读侧 CSP sandbox 兜底
//      （见 common/UploadFileServer.h）。
// ============================================================================
namespace ImageUploadRules {

/// 图片上传大小上限（5MB，与客户端一致）
inline constexpr size_t kMaxImageBytes = 5 * 1024 * 1024;

/// 提取并归一化 Content-Type：剥离参数、去首尾空白、转小写。
/// （RFC 2045 规定 MIME 大小写不敏感，避免 "Image/PNG" 被白名单误拒）
inline std::string extractMimeType(const std::string& ct) {
    auto p = ct.find(';');
    std::string m = (p == std::string::npos) ? ct : ct.substr(0, p);
    while (!m.empty() && (m.front() == ' ' || m.front() == '\t')) m.erase(0, 1);
    while (!m.empty() && (m.back() == ' ' || m.back() == '\t')) m.pop_back();
    for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return m;
}

/// 支持的图片 MIME 白名单。
/// 注意：不含 image/webp —— Qt 客户端无法渲染，禁止单边放开。
inline bool isSupportedImageType(const std::string& mime) {
    return mime == "image/jpeg" || mime == "image/jpg" || mime == "image/png"
        || mime == "image/gif" || mime == "image/bmp" || mime == "image/svg+xml";
}

/// MIME 类型 -> 文件扩展名（含前导 '.'），未知回退 ".jpg"。
inline std::string imageTypeToExtension(const std::string& mime) {
    if (mime == "image/png")      return ".png";
    if (mime == "image/gif")      return ".gif";
    if (mime == "image/bmp")      return ".bmp";
    if (mime == "image/svg+xml")  return ".svg";
    return ".jpg";
}

/// 大小写不敏感子串搜索（SVG 恶意特征扫描用）。
/// 输入只读、零拷贝；仅在 SVG 场景触发且黑名单极短，朴素循环已足够。
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

/// 通过文件头魔数识别真实图片类型；无法识别时返回空串。
/// 支持 JPEG / PNG / GIF / BMP，SVG 采用启发式（出现 <svg 且不命中恶意黑名单）。
/// 注：SVG 黑名单（<script、on*、javascript:）仅为第一道防线，盲区由读侧 CSP 兜底。
inline std::string sniffImageType(const std::string& content) {
    const size_t n = content.size();
    // JPEG: FF D8 FF
    if (n >= 3
        && static_cast<unsigned char>(content[0]) == 0xFF
        && static_cast<unsigned char>(content[1]) == 0xD8
        && static_cast<unsigned char>(content[2]) == 0xFF)
        return "image/jpeg";
    // PNG: 89 50 4E 47 0D 0A 1A 0A
    static constexpr unsigned char kPng[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n >= 8 && std::memcmp(content.data(), kPng, 8) == 0) return "image/png";
    // GIF: GIF87a / GIF89a
    if (n >= 6 && (content.compare(0, 6, "GIF87a") == 0 || content.compare(0, 6, "GIF89a") == 0))
        return "image/gif";
    // BMP: BM
    if (n >= 2 && content.compare(0, 2, "BM") == 0) return "image/bmp";

    // SVG 启发式检测（黑名单不承诺完备，权威防线见 UploadFileServer.h 的 CSP）
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

/// 图片上传校验状态码。
/// 由 classifyUpload() 返回，调用方（Handler）据此决定对客户端的响应行为。
enum class UploadError {
    None,              // 校验通过，允许继续落盘。
    UnsupportedType,   // MIME 类型不在白名单中（如 webp），拒绝上传。
    TooLarge,          // 文件大小超过 5MB 上限，拒绝上传。
    ContentMismatch    // 内容不可识别或格式异常（如伪装文件、恶意 SVG / SVGZ），拒绝上传。
};

/// 校验上传图片并决策落盘扩展名。
/// 执行顺序：白名单 -> 5MB 上限 -> 内容嗅探。
/// 嗅探采用“宽容修正”：声明与真实类型不符时，按真实类型落盘（避免误杀）。
/// 内容完全不可识别（含恶意 SVG / svgz / 伪装格式）时返回 ContentMismatch。
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

/// 将上传内容写入 /tmp 下唯一命名的临时文件。
/// @param tag 业务前缀（如 "avatar_1"），最终路径为 /tmp/gocook_<tag>_<时间戳><ext>。
/// @return 成功返回完整路径，失败（如无法打开文件）返回 nullopt。
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
