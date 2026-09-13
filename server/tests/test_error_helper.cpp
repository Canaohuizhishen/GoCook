// test_error_helper.cpp —— handleStandardException（common/ErrorHelper.h）的状态码映射契约
//
// 覆盖：
//   · ServiceException(503)（连接池饱和）：原样直通 503 + 原始文案——客户端据此识别
//     "服务器繁忙"并自动重试，语义不得被降级为 500
//   · 未识别状态码：按 500 兜底 + 通用文案（不透传内部错误信息）

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

} // namespace
