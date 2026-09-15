// test_error_helper.cpp —— handleStandardException（common/ErrorHelper.h）的状态码映射契约
//
// 覆盖：
//   · ServiceException(503)（连接池饱和）：原样直通 503 + 原始文案——客户端据此识别
//     "服务器繁忙"并自动重试，语义不得被降级为 500
//   · 未识别状态码：按 500 兜底 + 通用文案（不透传内部错误信息）
//   · nlohmann::json::exception：按可回溯来源分类（审查修复收敛为精确分类）——
//     – 请求体解析/字段读取（parse_error / out_of_range / 非 316 的 type_error）：400「请求体格式错误」，
//       请求体形状问题属客户端错误，不得落成 500；
//     – 序列化层非法 UTF-8（type_error.316）：500——请求体路径不可能产生它（parse 强制校验 UTF-8，
//       请求体里的非法 UTF-8 在解析层即被拒），只可能来自响应侧数据不变量被破坏。
//     两条分类依据各有用例钉住（含"请求体非法 UTF-8 落 400 分支"的反证用例）。

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

// 注：ErrorHelper.h 使用 gocook::services::ServiceException 但未自行 include，
// 依赖调用方（Handler）先引入 IServices——测试按同样的约定先包含 IServices 再包含 ErrorHelper
#include <gocook/IServices.h>

#include "common/ErrorHelper.h"

namespace {

TEST(ErrorHelperTest, 服务繁忙503原样直通) {
    httplib::Response res;
    try {
        throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 503) << "503 必须原样直通，不得被 default 分支降级为 500";
    EXPECT_EQ(res.get_header_value("Content-Type"), "application/json");
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "系统繁忙，请稍后重试");
}

TEST(ErrorHelperTest, 未识别状态码按500兜底) {
    httplib::Response res;
    try {
        throw gocook::services::ServiceException("内部细节不应对客户端可见", 418);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 500);
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "服务器内部错误，请稍后重试");
}

// ==================== 请求体 JSON 层异常 → 400（审查修复） ====================

TEST(ErrorHelperTest, JSON解析失败映射400) {
    httplib::Response res;
    try {
        auto j = nlohmann::json::parse("{invalid json");   // 抛 parse_error
        (void)j;
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "请求体格式错误");
}

TEST(ErrorHelperTest, 字段缺失out_of_range映射400) {
    httplib::Response res;
    try {
        nlohmann::json j = nlohmann::json::parse(R"({"a":1})");
        (void)j.at("b");                                  // 抛 out_of_range
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "请求体格式错误");
}

TEST(ErrorHelperTest, 字段类型不符type_error映射400) {
    httplib::Response res;
    try {
        nlohmann::json j = nlohmann::json::parse(R"({"a":"x"})");
        (void)j["a"].get<int>();                          // 抛 type_error
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "请求体格式错误");
}

// ==================== 序列化层非法 UTF-8 → 500（防 400 误分类，审查修复） ====================

TEST(ErrorHelperTest, 序列化层非法UTF8映射500) {
    httplib::Response res;
    try {
        nlohmann::json j;
        j["s"] = std::string(1, static_cast<char>(0xFF));   // 程序内构造非法 UTF-8（请求体路径到不了这里）
        (void)j.dump();                                     // 抛 type_error.316
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 500) << "序列化层非法 UTF-8 属服务端故障，不得伪装成 400";
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "服务器内部错误，请稍后重试");
}

TEST(ErrorHelperTest, 请求体非法UTF8在解析层即被拒映射400) {
    // 反证用例：钉住 316 分类的依据——parse 强制校验 UTF-8，请求体里的非法 UTF-8 在解析层
    // 就抛 parse_error（落 400 分支）；因此 id==316 只可能来自响应序列化，500 分支不会被
    // 请求体路径触发。
    httplib::Response res;
    try {
        std::string body = "{\"s\":\"a";
        body += static_cast<char>(0xFF);                    // 非法 UTF-8 字节
        body += " b\"}";
        auto j = nlohmann::json::parse(body);               // 抛 parse_error（ill-formed UTF-8 byte）
        (void)j;
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(nlohmann::json::parse(res.body)["error"].get<std::string>(), "请求体格式错误");
}

} // namespace
