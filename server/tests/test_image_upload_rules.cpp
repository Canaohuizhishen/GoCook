// test_image_upload_rules.cpp —— ImageUploadRules（图片上传规则收口）单元测试
//
// 覆盖：
//   · 白名单：六类图片类型放行；webp 拒绝（Qt 客户端不支持渲染，回归保护）
//   · Content-Type → 扩展名映射（未知回退 .jpg）
//   · extractMimeType：剥 ";boundary=..." 参数并 trim、统一转小写（MIME 大小写不敏感）
//   · 魔数嗅探：jpeg/png/gif87a/gif89a/bmp/svg；svg 含 <script>/on* 事件属性/
//     javascript: URI（均含大写变体）拒绝；svgz（gzip 魔数）等无法识别返回空
//   · classifyUpload 宽容修正：声明与内容不符时按真实类型落盘；伪装格式拒绝
//   · classifyUpload 超限：白名单 → 5MB → 嗅探 的检查顺序（TooLarge 先于内容扫描）
//   · kMaxImageBytes = 5MB；writeTempImageFile 落盘可读回
//
// 纯函数测试，不依赖数据库与网络。

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "../common/ImageUploadRules.h"

using ImageUploadRules::UploadError;

namespace {

// 构造真实图片魔数前缀的测试内容
const std::string kJpegBytes = "\xFF\xD8\xFF\xE0\x00\x10JFIF";
const std::string kPngBytes  = "\x89PNG\r\n\x1a\n" + std::string("\0", 1) + "data";
const std::string kGifBytes  = "GIF89a...";
const std::string kBmpBytes  = "BM...";
const std::string kSvgText   = "<?xml version=\"1.0\"?><svg xmlns=\"http://www.w3.org/2000/svg\"><rect/></svg>";
const std::string kGzipSvgz  = "\x1F\x8B\x08\x00\x00\x00\x00\x00gzip-svg";

std::string readAll(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

} // namespace

TEST(ImageUploadRulesTest, 白名单接受六类图片拒绝webp等) {
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/jpeg"));
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/jpg"));
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/png"));
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/gif"));
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/bmp"));
    EXPECT_TRUE(ImageUploadRules::isSupportedImageType("image/svg+xml"));
    // Qt 客户端不支持渲染 WebP —— 写侧白名单故意不含它，勿单边放开
    EXPECT_FALSE(ImageUploadRules::isSupportedImageType("image/webp"));
    EXPECT_FALSE(ImageUploadRules::isSupportedImageType("text/html"));
    EXPECT_FALSE(ImageUploadRules::isSupportedImageType("image/svg"));
    EXPECT_FALSE(ImageUploadRules::isSupportedImageType(""));
}

TEST(ImageUploadRulesTest, MIME到扩展名映射未知回退jpg) {
    EXPECT_EQ(".png", ImageUploadRules::imageTypeToExtension("image/png"));
    EXPECT_EQ(".gif", ImageUploadRules::imageTypeToExtension("image/gif"));
    EXPECT_EQ(".bmp", ImageUploadRules::imageTypeToExtension("image/bmp"));
    EXPECT_EQ(".svg", ImageUploadRules::imageTypeToExtension("image/svg+xml"));
    EXPECT_EQ(".jpg", ImageUploadRules::imageTypeToExtension("image/jpeg"));
    EXPECT_EQ(".jpg", ImageUploadRules::imageTypeToExtension("image/jpg"));
    EXPECT_EQ(".jpg", ImageUploadRules::imageTypeToExtension("image/webp"));  // 理论不可达（白名单拦截）
}

TEST(ImageUploadRulesTest, extractMimeType剥参数与trim) {
    EXPECT_EQ("image/png", ImageUploadRules::extractMimeType("image/png; boundary=----abc"));
    EXPECT_EQ("image/jpeg", ImageUploadRules::extractMimeType(" image/jpeg "));
    EXPECT_EQ("image/svg+xml", ImageUploadRules::extractMimeType("image/svg+xml"));
    EXPECT_EQ("multipart/form-data", ImageUploadRules::extractMimeType("multipart/form-data; boundary=x"));
    EXPECT_EQ("", ImageUploadRules::extractMimeType(""));
}

TEST(ImageUploadRulesTest, extractMimeType大小写不敏感) {
    // MIME 类型按 RFC 2045 大小写不敏感："Image/PNG" 等写法应归一为小写再比对白名单
    EXPECT_EQ("image/png", ImageUploadRules::extractMimeType("Image/PNG"));
    EXPECT_EQ("image/jpeg", ImageUploadRules::extractMimeType("image/JPEG"));
    EXPECT_EQ("image/svg+xml", ImageUploadRules::extractMimeType(" IMAGE/SVG+XML ; boundary=x"));
    EXPECT_EQ("image/bmp", ImageUploadRules::extractMimeType("Image/BMP; charset=binary"));

    // 归一化后与白名单/扩展名映射配合：大写声明不再被误拒为"不支持的图片格式"
    // （handler 调用链中 extractMimeType 先行归一，classifyUpload 按归一后值比对）
    std::string ext;
    EXPECT_EQ(UploadError::None,
              ImageUploadRules::classifyUpload(kPngBytes,
                                               ImageUploadRules::extractMimeType("IMAGE/PNG"),
                                               ext));
    EXPECT_EQ(".png", ext);

    // 未归一化的声明仍被拒（归一职责在 extractMimeType 单点）
    EXPECT_EQ(UploadError::UnsupportedType,
              ImageUploadRules::classifyUpload(kPngBytes, "IMAGE/PNG", ext));
}

TEST(ImageUploadRulesTest, 魔数识别各真实格式) {
    EXPECT_EQ("image/jpeg", ImageUploadRules::sniffImageType(kJpegBytes));
    EXPECT_EQ("image/png", ImageUploadRules::sniffImageType(kPngBytes));
    EXPECT_EQ("image/gif", ImageUploadRules::sniffImageType(kGifBytes));
    EXPECT_EQ("image/gif", ImageUploadRules::sniffImageType("GIF87a-old"));
    EXPECT_EQ("image/bmp", ImageUploadRules::sniffImageType(kBmpBytes));
    EXPECT_EQ("image/svg+xml", ImageUploadRules::sniffImageType(kSvgText));
    EXPECT_EQ("image/svg+xml", ImageUploadRules::sniffImageType("<svg/>"));
    // 纯文本 / gzip 压缩 svg（svgz）/ 空内容均不可识别
    EXPECT_EQ("", ImageUploadRules::sniffImageType("hello world"));
    EXPECT_EQ("", ImageUploadRules::sniffImageType(kGzipSvgz));
    EXPECT_EQ("", ImageUploadRules::sniffImageType(""));
}

TEST(ImageUploadRulesTest, svg含脚本被拒大小写均检测) {
    const std::string evil = "<svg xmlns=\"http://www.w3.org/2000/svg\">"
                             "<script>alert(1)</script></svg>";
    const std::string evilUpper = "<svg xmlns=\"http://www.w3.org/2000/svg\">"
                                  "<SCRIPT>alert(1)</SCRIPT></svg>";
    EXPECT_EQ("", ImageUploadRules::sniffImageType(evil));
    EXPECT_EQ("", ImageUploadRules::sniffImageType(evilUpper));
}

TEST(ImageUploadRulesTest, svg含事件处理器或javascriptURI被拒) {
    // 无 <script、仅事件处理器属性的 svg（经典 onload 载荷）——修复前会放行
    const std::string onload = "<svg xmlns=\"http://www.w3.org/2000/svg\" onload=\"alert(1)\"/>";
    const std::string onloadUpper = "<svg ONLOAD=\"alert(1)\" xmlns=\"http://www.w3.org/2000/svg\"/>";
    const std::string jsHref = "<svg xmlns=\"http://www.w3.org/2000/svg\">"
                               "<a href=\"javascript:alert(1)\">x</a></svg>";
    EXPECT_EQ("", ImageUploadRules::sniffImageType(onload));
    EXPECT_EQ("", ImageUploadRules::sniffImageType(onloadUpper));
    EXPECT_EQ("", ImageUploadRules::sniffImageType(jsHref));

    // 常见合法 svg（viewBox/gradient/style/文本）不误杀
    const std::string legit = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">"
                              "<defs><linearGradient id=\"g\"><stop offset=\"0\"/></linearGradient></defs>"
                              "<rect fill=\"url(#g)\"/><text>hello</text></svg>";
    EXPECT_EQ("image/svg+xml", ImageUploadRules::sniffImageType(legit));
}

TEST(ImageUploadRulesTest, classify宽容修正不绕过恶意svg) {
    // 声明 jpeg、内容为带 onload 的 svg → 仍 ContentMismatch（宽容修正不是 XSS 后门）
    const std::string evil = "<svg xmlns=\"http://www.w3.org/2000/svg\" onload=\"alert(1)\"/>";
    std::string ext = "UNTOUCHED";
    EXPECT_EQ(UploadError::ContentMismatch,
              ImageUploadRules::classifyUpload(evil, "image/jpeg", ext));
    EXPECT_EQ("UNTOUCHED", ext);
}

TEST(ImageUploadRulesTest, classify宽容修正按真实类型落盘) {
    std::string ext;

    // 声明 jpeg、内容 png → 落盘 .png（客户端按扩展名猜 Content-Type 的场景）
    EXPECT_EQ(UploadError::None, ImageUploadRules::classifyUpload(kPngBytes, "image/jpeg", ext));
    EXPECT_EQ(".png", ext);

    // 声明 png、内容 jpeg → 落盘 .jpg
    EXPECT_EQ(UploadError::None, ImageUploadRules::classifyUpload(kJpegBytes, "image/png", ext));
    EXPECT_EQ(".jpg", ext);

    // 声明 svg、内容 svg 文本 → .svg
    EXPECT_EQ(UploadError::None, ImageUploadRules::classifyUpload(kSvgText, "image/svg+xml", ext));
    EXPECT_EQ(".svg", ext);
}

TEST(ImageUploadRulesTest, classify拒绝未白名单与不可识别内容) {
    std::string ext = "UNTOUCHED";

    // 声明类型不在白名单（webp）→ UnsupportedType，扩展名不动
    EXPECT_EQ(UploadError::UnsupportedType,
              ImageUploadRules::classifyUpload(kJpegBytes, "image/webp", ext));
    EXPECT_EQ("UNTOUCHED", ext);

    // 伪装格式：声明 png、内容不可识别 → ContentMismatch
    EXPECT_EQ(UploadError::ContentMismatch,
              ImageUploadRules::classifyUpload("not-an-image", "image/png", ext));
    EXPECT_EQ("UNTOUCHED", ext);

    // 声明 svg、内容带脚本 → ContentMismatch（stored-XSS 载体拦截）
    const std::string evil = "<svg><script>alert(1)</script></svg>";
    EXPECT_EQ(UploadError::ContentMismatch,
              ImageUploadRules::classifyUpload(evil, "image/svg+xml", ext));
}

TEST(ImageUploadRulesTest, 大小上限为5MB) {
    EXPECT_EQ(size_t{5} * 1024 * 1024, ImageUploadRules::kMaxImageBytes);
}

TEST(ImageUploadRulesTest, classify超限先于嗅探拒绝) {
    std::string ext = "UNTOUCHED";
    const std::string big(ImageUploadRules::kMaxImageBytes + 1, 'A');

    // 超限 + 声明合法：TooLarge（不做全量嗅探，扩展名不动）
    EXPECT_EQ(UploadError::TooLarge,
              ImageUploadRules::classifyUpload(big, "image/png", ext));
    EXPECT_EQ("UNTOUCHED", ext);

    // 超限 + 内容不可识别：仍 TooLarge（嗅探排在大小检查之后）
    EXPECT_EQ(UploadError::TooLarge,
              ImageUploadRules::classifyUpload(big, "image/svg+xml", ext));

    // 错误优先级：声明不在白名单（webp）→ UnsupportedType（白名单先于大小检查）
    EXPECT_EQ(UploadError::UnsupportedType,
              ImageUploadRules::classifyUpload(big, "image/webp", ext));

    // 边界：恰好 5MB + 合法魔数 → None（上限为严格大于）
    const std::string atLimit = kJpegBytes
        + std::string(ImageUploadRules::kMaxImageBytes - kJpegBytes.size(), 'X');
    EXPECT_EQ(UploadError::None, ImageUploadRules::classifyUpload(atLimit, "image/jpeg", ext));
    EXPECT_EQ(".jpg", ext);
}

TEST(ImageUploadRulesTest, writeTempImageFile落盘可读回) {
    auto pathOpt = ImageUploadRules::writeTempImageFile("rules_ut", "file-content", ".png");
    ASSERT_TRUE(pathOpt.has_value());
    EXPECT_EQ(".png", pathOpt->substr(pathOpt->size() - 4));
    EXPECT_TRUE(std::filesystem::exists(*pathOpt));

    EXPECT_EQ("file-content", readAll(*pathOpt));
    std::filesystem::remove(*pathOpt);  // 清理临时文件
}
