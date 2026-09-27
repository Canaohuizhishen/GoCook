// test_pagination_helper.cpp —— PaginationHelper（common/PaginationHelper.h）参数解析契约测试
//
// 背景（审查修复）：推荐「换一批」的 seed 是客户端 QRandomGenerator 生成的全 32 位无符号值，
// 此前经 std::stoi 的 int 中转解析——>2^31-1 的值被误判溢出而静默归 0，
// 约半数换批退回首屏批次（"换批失灵"根因）。本文件锁定 parseUIntParam 的边界语义：
//   · 0 ~ 4294967295 内纯数字原样返回（含 2147483648 / 3000000000 两个回归点）
//   · 超出 4294967295 / 超 long long 长数字 / 负号 / 带符号写法（+5、前导空白）/ 非数字 → 返回默认值
//   · 参数缺失或为空 → 返回默认值
//
// 实现要点（勿退回）：std::stoul 对 "-1" 这类负值按回绕处理而不是报错，
// 故实现先做"纯数字"预检——本文件同时锁定该行为（-1 必须归默认，不得回绕成 4294967295）。

#include <gtest/gtest.h>

#include <string>

#include <httplib/httplib.h>

#include "common/PaginationHelper.h"

namespace {

/// 构造带指定参数的请求（httplib::Request::params 与真实查询解析的产物同构）
httplib::Request withParam(const std::string& name, const std::string& value) {
    httplib::Request req;
    req.params.emplace(name, value);
    return req;
}

} // namespace

// ==================== 有效值：原样返回 ====================

TEST(PaginationHelperTest, 无符号解析常规值与零原样返回) {
    EXPECT_EQ(parseUIntParam(withParam("seed", "42"), "seed", 0), 42u);
    EXPECT_EQ(parseUIntParam(withParam("seed", "0"), "seed", 0), 0u);
}

TEST(PaginationHelperTest, 无符号解析INT_MAX边界保持有效) {
    EXPECT_EQ(parseUIntParam(withParam("seed", "2147483647"), "seed", 0), 2147483647u);
}

TEST(PaginationHelperTest, 无符号解析超过INT_MAX仍有效回归点) {
    // 曾经：经 int 中转，这两个值被 std::stoi 判溢出而静默归 0（换批失灵根因）
    EXPECT_EQ(parseUIntParam(withParam("seed", "2147483648"), "seed", 0), 2147483648u);
    EXPECT_EQ(parseUIntParam(withParam("seed", "3000000000"), "seed", 0), 3000000000u);
}

TEST(PaginationHelperTest, 无符号解析UINT32上限接受) {
    EXPECT_EQ(parseUIntParam(withParam("seed", "4294967295"), "seed", 0), 4294967295u);
}

// ==================== 无效值：静默归默认 ====================

TEST(PaginationHelperTest, 无符号解析超出UINT32上限归默认) {
    EXPECT_EQ(parseUIntParam(withParam("seed", "4294967296"), "seed", 0), 0u);
    // 超过 unsigned long long（约 1.8e19）的长数字由 stoull 抛错兜住
    EXPECT_EQ(parseUIntParam(withParam("seed", "99999999999999999999999"), "seed", 0), 0u);
    // 默认值透传
    EXPECT_EQ(parseUIntParam(withParam("seed", "4294967296"), "seed", 7), 7u);
}

TEST(PaginationHelperTest, 无符号解析负号与带符号写法归默认) {
    // std::stoul("-1") 会回绕成 ULONG_MAX 而不是报错——实现必须靠纯数字预检拦住
    EXPECT_EQ(parseUIntParam(withParam("seed", "-1"), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", "-4294967295"), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", "+5"), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", " 5"), "seed", 0), 0u);
}

TEST(PaginationHelperTest, 无符号解析非数字与空值归默认) {
    EXPECT_EQ(parseUIntParam(withParam("seed", "abc"), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", "3.14"), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", ""), "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(withParam("seed", ""), "seed", 9), 9u);
}

TEST(PaginationHelperTest, 无符号解析参数缺失返回默认值) {
    httplib::Request req;
    EXPECT_EQ(parseUIntParam(req, "seed", 0), 0u);
    EXPECT_EQ(parseUIntParam(req, "seed", 7), 7u);
}
