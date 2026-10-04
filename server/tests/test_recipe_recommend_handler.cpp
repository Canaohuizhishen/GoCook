// test_recipe_recommend_handler.cpp —— 智能推荐（Handler 层：seed 解析链路）测试
//
// 覆盖缺口：推荐「换一批」此前的测试分布是服务端 5 例（Service 层直调）
// + 客户端 2 例（桩服务）——"HTTP 查询参数 → seed 解析 → Service"这一夹缝层没有任何测试。
// 由 int 中转解析导致的"大 seed（>2^31-1）溢出静默归 0、约半数换批退回首屏批次"
// 即从该缝隙漏出。本文件补齐该层：参数进来、批次真的变了。
//
// 覆盖（httplib 真实路由 + 自签 JWT + 真 RecipeServiceImpl + Mock 仓库）：
//   · 鉴权：无令牌 → 401「无效的访问令牌」（首次覆盖 recommend 路由的鉴权出口）
//   · 缺省 seed 与显式 seed=0 响应逐位一致（确定性首屏，穿过 HTTP 链）
//   · 大 seed（>2^31-1）必须改变批次：3000000000/3000000001/3000000002 至少一个与首屏不同
//     （行为锁；若解析退回 int 中转 → 三值全部归 0 → 与首屏逐位相同 → 用例必挂）
//   · 同一大 seed 两次请求响应逐位一致（可复现性穿过 HTTP 链）
//
// 桩数据：10 个复合分严格递减、风味/技法两两不同的候选（size=6 时多样化约束不干扰批次断言，
// 与 test_recipe_service.cpp 换批用例同套路）；批次差异断言依赖 libstdc++ shuffle 映射
// （与既有"换一批"服务层用例同性质的行为锁）。

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <httplib/httplib.h>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "../handlers/RecipeHandler.h"
#include "../middleware/auth_middleware.h"
#include "../services/RecipeServiceImpl.h"
#include "MockInventoryRepository.h"
#include "MockRecipeRepository.h"
#include "MockUserRepository.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using gocook::models::PagedInventory;
using gocook::models::PagedRecommendedRecipes;
using gocook::models::RecommendedRecipe;
using gocook::models::UserPreferences;

namespace {

constexpr const char* kTestSecret = "recipe-recommend-handler-test-secret";
constexpr int kUserId = 7;

/// 自签 JWT：iss=GoCook／userId／username／role／exp；刻意不携带 ver——
/// 覆盖「ver 缺失按 0」的兼容路径（mock 仓库默认返回版本 0，与真实令牌校验等价）
std::string mintToken(int userId) {
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer("GoCook")
        .set_type("JWS")
        .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
        .set_payload_claim("username", jwt::claim(std::string("tester")))
        .set_payload_claim("role", jwt::claim(std::string("user")))
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .sign(jwt::algorithm::hs256{kTestSecret});
}

// 与 Router 注册完全一致的推荐路由（直测 Handler）
class HandlerTestServer {
public:
    explicit HandlerTestServer(RecipeHandler& handler) {
        svr_.Get("/api/recipes/recommend",
                 [&handler](const httplib::Request& req, httplib::Response& res) {
                     handler.getRecommendedRecipes(req, res);
                 });
        port_ = svr_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0)
            throw std::runtime_error("HandlerTestServer: bind_to_any_port failed");
        thread_ = std::thread([this] { svr_.listen_after_bind(); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!svr_.is_running() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!svr_.is_running())
            throw std::runtime_error("HandlerTestServer: failed to start");
    }

    ~HandlerTestServer() {
        svr_.stop();
        if (thread_.joinable()) thread_.join();
    }

    httplib::Client client() const { return httplib::Client("127.0.0.1", port_); }

private:
    httplib::Server svr_;
    int port_ = 0;
    std::thread thread_;
};

/// 候选构造：风味/技法两两不同、复合分严格递减（同 test_recipe_service.cpp 的换批辅助思路）
RecommendedRecipe makeRec(int id, const std::string& flavor, const std::string& method,
                          double matchScore) {
    RecommendedRecipe r;
    r.id = id;
    r.name = "推荐菜" + std::to_string(id);
    r.flavor = flavor;
    r.cooking_method = method;
    r.ingredient_type = "荤";
    r.match_score = matchScore;
    r.avg_rating = 4.0;
    r.view_count = 100;
    r.calories = 350;
    r.protein_g = 18;
    r.fat_g = 12;
    r.carbs_g = 30;
    r.submitted_at = "2026-05-20T00:00:00";   // 早于 7 天窗口，新鲜度加分为 0（各菜一致）
    r.description = "Test";
    r.prep_time_minutes = 10;
    r.cook_time_minutes = 20;
    r.author_id = 1;
    r.author_name = "Chef";
    r.match_status.available_ingredients.push_back({"鸡蛋", 3.0, "个"});
    r.match_status.missing_ingredients.push_back({"葱花", 1.0, "把"});
    return r;
}

PagedRecommendedRecipes makeCandidates(int count) {
    static const char* kFlavors[] = {"清淡", "麻辣", "酸甜", "蒜香", "酸辣",
                                     "酱香", "葱香", "鲜香", "椒香", "糟香"};
    static const char* kMethods[] = {"蒸", "炒", "炖", "煮", "拌", "烧", "煸", "炸", "烤", "焖"};
    PagedRecommendedRecipes r;
    for (int i = 0; i < count; ++i)
        r.data.push_back(makeRec(i + 1, kFlavors[i % 10], kMethods[i % 10], 0.95 - i * 0.05));
    r.pagination = {1, count, count, 1};
    return r;
}

PagedInventory makePagedInventory(int total) {
    PagedInventory r;
    r.pagination = {1, 1, total, total > 0 ? 1 : 0};
    return r;
}

} // namespace

class RecipeRecommendHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
        auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
        auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
        recipeRepo_ = mockRecipe.get();
        userRepo_ = mockUser.get();
        invRepo_ = mockInv.get();

        service_ = std::make_unique<RecipeServiceImpl>(
            std::move(mockRecipe), std::move(mockUser), std::move(mockInv));
        // 缺 ver 的测试令牌按 0 校验 → mock 返回版本 0（JWT 主动吊销的兼容路径）
        ON_CALL(*userRepo_, getTokenVersion(_)).WillByDefault(Return(std::optional<int>{0}));
        auth_ = std::make_unique<AuthMiddleware>(kTestSecret, *userRepo_);
        handler_ = std::make_unique<RecipeHandler>(*service_, *auth_);
        // 最后构造 → 最先析构：先停 HTTP 服务（其在途请求持有 handler 引用）再拆依赖
        srv_ = std::make_unique<HandlerTestServer>(*handler_);
        token_ = "Bearer " + mintToken(kUserId);
    }

    /// 常用桩：库存非空 + 空偏好/无健康档案 + 固定候选集（服务端按全库上限 300 取数）
    void stubDefaults(const PagedRecommendedRecipes& candidates) {
        EXPECT_CALL(*invRepo_, findInventory(kUserId, 1, 1))
            .WillRepeatedly(Return(makePagedInventory(1)));
        EXPECT_CALL(*userRepo_, getPreferences(kUserId))
            .WillRepeatedly(Return(UserPreferences{}));
        EXPECT_CALL(*userRepo_, getHealthConditions(kUserId))
            .WillRepeatedly(Return(std::vector<std::string>{}));
        EXPECT_CALL(*recipeRepo_, findRecommendedRecipes(kUserId, 1, 300))
            .WillRepeatedly(Return(candidates));
    }

    httplib::Headers authHeaders() const { return {{"Authorization", token_}}; }

    NiceMock<MockRecipeRepository>* recipeRepo_ = nullptr;
    NiceMock<MockUserRepository>* userRepo_ = nullptr;
    NiceMock<MockInventoryRepository>* invRepo_ = nullptr;
    std::unique_ptr<RecipeServiceImpl> service_;
    std::unique_ptr<AuthMiddleware> auth_;
    std::unique_ptr<RecipeHandler> handler_;
    std::unique_ptr<HandlerTestServer> srv_;
    std::string token_;
};

TEST_F(RecipeRecommendHandlerTest, 无令牌返回401且文案为无效令牌) {
    auto cli = srv_->client();
    auto res = cli.Get("/api/recipes/recommend?page=1&size=6");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 401);
    EXPECT_NE(res->body.find("无效的访问令牌"), std::string::npos);
}

TEST_F(RecipeRecommendHandlerTest, 缺省种子与零种子响应逐位一致且批次降序) {
    stubDefaults(makeCandidates(10));
    auto cli = srv_->client();

    auto resDefault = cli.Get("/api/recipes/recommend?page=1&size=6", authHeaders());
    auto resZero = cli.Get("/api/recipes/recommend?page=1&size=6&seed=0", authHeaders());
    ASSERT_TRUE(resDefault);
    ASSERT_TRUE(resZero);
    ASSERT_EQ(resDefault->status, 200);
    ASSERT_EQ(resZero->status, 200);
    EXPECT_EQ(resDefault->body, resZero->body) << "缺省 seed 必须等价于 seed=0（确定性首屏）";

    // 抽样确认响应结构与降序不变量（防响应体结构漂移导致后续断言失真）
    auto j = nlohmann::json::parse(resDefault->body);
    ASSERT_EQ(j["data"].size(), 6u);
    double prev = 2.0;
    for (const auto& item : j["data"]) {
        const double s = item["match_score"].get<double>();
        EXPECT_LE(s, prev);
        prev = s;
    }
}

TEST_F(RecipeRecommendHandlerTest, 大种子改变批次且同种子可复现) {
    stubDefaults(makeCandidates(10));
    auto cli = srv_->client();

    auto baseline = cli.Get("/api/recipes/recommend?page=1&size=6", authHeaders());
    ASSERT_TRUE(baseline);
    ASSERT_EQ(baseline->status, 200);

    // 全 32 位无符号种子（>2^31-1）：若解析退回 int 中转，三值全部静默归 0 → 与首屏逐位相同
    const unsigned int kBigSeeds[] = {3000000000u, 3000000001u, 3000000002u};
    bool anyDifferent = false;
    for (unsigned int seed : kBigSeeds) {
        const std::string url =
            "/api/recipes/recommend?page=1&size=6&seed=" + std::to_string(seed);
        auto res = cli.Get(url, authHeaders());
        ASSERT_TRUE(res) << "seed=" << seed;
        ASSERT_EQ(res->status, 200) << "seed=" << seed;
        if (res->body != baseline->body) anyDifferent = true;
    }
    EXPECT_TRUE(anyDifferent)
        << "全部大种子退化为首屏批次（seed 解析回归：可能又被 int 中转吞掉了）";

    // 同一大种子两次请求必须逐位一致（可复现性穿过完整 HTTP 链）
    const std::string url =
        "/api/recipes/recommend?page=1&size=6&seed=" + std::to_string(kBigSeeds[0]);
    auto first = cli.Get(url, authHeaders());
    auto again = cli.Get(url, authHeaders());
    ASSERT_TRUE(first);
    ASSERT_TRUE(again);
    ASSERT_EQ(first->status, 200);
    EXPECT_EQ(first->body, again->body);
}
