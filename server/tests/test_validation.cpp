// test_validation.cpp —— Validation（common/Validation.h）收藏批量端点的请求形状校验契约
//
// 背景（审查修复）：缺 favorite_ids / 字段类型错误的请求此前落到 500；现由两层收口——
//   · 本文件覆盖的 Validation 校验函数：给出 400 精确文案（房规同 validateRegisterRequest 等）
//   · ErrorHelper 单一守卫：json::exception（解析失败/字段读取异常）统一映射 400「请求体格式错误」（见 test_error_helper.cpp）
//
// 覆盖 3.5 批量更新 / 3.6 批量删除：
//   · favorite_ids：必填、必须为整数数组、元素必须在 int 范围内（缺键/非数组/元素越界均 400）
//   · group_id / is_public：可选；出现即必须类型正确（整数 / 布尔）
//   · 空列表（3.5 / 3.6 同为 400「favorite_ids 不能为空」）与"无更新字段"不在本层判定——解析通过，由服务层收口
//     （服务层用例见 test_user_service.cpp：批量更新收藏项空ID列表被拒 / 批量删除收藏空ID列表被拒）

#include <gtest/gtest.h>

#include <string>

#include <nlohmann/json.hpp>
#include <gocook/IServices.h>

#include "common/Validation.h"

namespace {

/// 断言校验被拒：抛 ServiceException(400) 且消息为预期精确文案
template <typename Fn>
void expectRejected(Fn&& fn, const std::string& expectedMsg) {
    try {
        fn();
        FAIL() << "期望 ServiceException(400) 但未抛出";
    } catch (const gocook::services::ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), expectedMsg);
    }
}

nlohmann::json parseJson(const char* s) { return nlohmann::json::parse(s); }

} // namespace

// ==================== 批量更新收藏（3.5） ====================

TEST(ValidationTest, 批量更新全字段合法通过) {
    EXPECT_TRUE(Validation::validateBatchUpdateFavoritesRequest(
        parseJson(R"({"favorite_ids":[1,2,5],"group_id":3,"is_public":false})")));
}

TEST(ValidationTest, 批量更新仅必填字段合法通过) {
    EXPECT_TRUE(Validation::validateBatchUpdateFavoritesRequest(
        parseJson(R"({"favorite_ids":[1]})")));
}

TEST(ValidationTest, 批量更新缺favorite_ids被拒) {
    expectRejected(
        [] { Validation::validateBatchUpdateFavoritesRequest(parseJson(R"({"group_id":3})")); },
        "缺少 favorite_ids 字段");
}

TEST(ValidationTest, 批量更新顶层非对象按缺字段被拒) {
    expectRejected(
        [] { Validation::validateBatchUpdateFavoritesRequest(parseJson("[1,2]")); },
        "缺少 favorite_ids 字段");
}

TEST(ValidationTest, 批量更新favorite_ids非数组被拒) {
    expectRejected(
        [] { Validation::validateBatchUpdateFavoritesRequest(parseJson(R"({"favorite_ids":"1"})")); },
        "favorite_ids 必须为整数数组");
}

TEST(ValidationTest, 批量更新favorite_ids元素非整数被拒) {
    for (const char* body : {R"({"favorite_ids":["1"]})",
                             R"({"favorite_ids":[1.5]})",
                             R"({"favorite_ids":[true]})",
                             R"({"favorite_ids":[null]})"}) {
        expectRejected(
            [body] { Validation::validateBatchUpdateFavoritesRequest(parseJson(body)); },
            "favorite_ids 必须为整数数组");
    }
}

TEST(ValidationTest, 批量更新favorite_ids元素越界被拒) {
    // 3000000000 超 int 上界；18446744073709551615 = uint64 上界（防 get<int>() 静默回绕）
    for (const char* body : {R"({"favorite_ids":[3000000000]})",
                             R"({"favorite_ids":[-3000000000]})",
                             R"({"favorite_ids":[18446744073709551615]})"}) {
        expectRejected(
            [body] { Validation::validateBatchUpdateFavoritesRequest(parseJson(body)); },
            "favorite_ids 必须为整数数组");
    }
}

TEST(ValidationTest, 批量更新负id在int范围内放行由SQL静默跳过) {
    EXPECT_TRUE(Validation::validateBatchUpdateFavoritesRequest(
        parseJson(R"({"favorite_ids":[-5]})")));
}

TEST(ValidationTest, 批量更新group_id非整数被拒) {
    expectRejected(
        [] { Validation::validateBatchUpdateFavoritesRequest(
                 parseJson(R"({"favorite_ids":[1],"group_id":"3"})")); },
        "group_id 必须为整数");
}

TEST(ValidationTest, 批量更新group_id越界被拒) {
    expectRejected(
        [] { Validation::validateBatchUpdateFavoritesRequest(
                 parseJson(R"({"favorite_ids":[1],"group_id":3000000000})")); },
        "group_id 必须为整数");
}

TEST(ValidationTest, 批量更新is_public非布尔被拒) {
    for (const char* body : {R"({"favorite_ids":[1],"is_public":1})",
                             R"({"favorite_ids":[1],"is_public":"false"})"}) {
        expectRejected(
            [body] { Validation::validateBatchUpdateFavoritesRequest(parseJson(body)); },
            "is_public 必须为布尔值");
    }
}

TEST(ValidationTest, 批量更新空列表与无更新字段属服务层语义本层放行) {
    // 形状层通过：空列表 → 服务层 400「favorite_ids 不能为空」；无更新字段 → 服务层 400「缺少更新字段」
    EXPECT_TRUE(Validation::validateBatchUpdateFavoritesRequest(parseJson(R"({"favorite_ids":[]})")));
    EXPECT_TRUE(Validation::validateBatchUpdateFavoritesRequest(parseJson(R"({"favorite_ids":[1]})")));
}

// ==================== 批量删除收藏（3.6） ====================

TEST(ValidationTest, 批量删除合法通过) {
    EXPECT_TRUE(Validation::validateBatchDeleteFavoritesRequest(
        parseJson(R"({"favorite_ids":[1,2,3]})")));
}

TEST(ValidationTest, 批量删除缺favorite_ids被拒) {
    expectRejected(
        [] { Validation::validateBatchDeleteFavoritesRequest(parseJson("{}")); },
        "缺少 favorite_ids 字段");
}

TEST(ValidationTest, 批量删除favorite_ids非数组被拒) {
    expectRejected(
        [] { Validation::validateBatchDeleteFavoritesRequest(parseJson(R"({"favorite_ids":1})")); },
        "favorite_ids 必须为整数数组");
}
