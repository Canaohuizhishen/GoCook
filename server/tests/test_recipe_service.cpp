#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../services/RecipeServiceImpl.h"
#include "MockRecipeRepository.h"
#include "MockUserRepository.h"
#include "MockInventoryRepository.h"

using namespace testing;
using namespace gocook::models;
using namespace gocook::services;
using namespace gocook::repository;

namespace {
    RecipeSummary makeSummary(int id = 1) {
        return {id, "Test Recipe", "A delicious test", "img.jpg",
                "炒", "清淡", "荤", 10, 20, 500, 100, 4.5, {"test"}, 1, "Chef"};
    }

    PagedRecipes makePagedRecipes(int count = 1) {
        PagedRecipes result;
        for (int i = 1; i <= count; ++i)
            result.data.push_back(makeSummary(i));
        result.pagination = {1, count, 10, 1};
        return result;
    }

    RecipeDetail makeDetail(int id = 1) {
        return {id, "Test Recipe", "Detailed description", "img.jpg",
                "炒", "清淡", 10, 20, 100, 4.5, false, {}, {}, {}, {"test"}, 1, "Chef", "2026-01-01"};
    }

    SubmitRecipeRequest makeSubmitReq() {
        return {"New Recipe", "Yummy", "img.jpg", {}, {}, std::nullopt, {}, std::nullopt, std::nullopt, std::nullopt};
    }

    // 推荐测试的辅助构造
    RecommendedRecipe makeRec(int id, const std::string& name,
                              const std::string& flavor,
                              const std::string& method,
                              double matchScore = 0.8,
                              double avgRating = 4.0)
    {
        RecommendedRecipe r;
        r.id = id;
        r.name = name;
        r.flavor = flavor;
        r.cooking_method = method;
        r.ingredient_type = "荤";
        r.match_score = matchScore;
        r.avg_rating = avgRating;
        r.view_count = 100;
        r.calories = 350;
        r.protein_g = 18;
        r.fat_g = 12;
        r.carbs_g = 30;
        r.submitted_at = "2026-05-20T00:00:00";
        r.description = "Test";
        r.prep_time_minutes = 10;
        r.cook_time_minutes = 20;
        r.author_id = 1;
        r.author_name = "Chef";
        // 添加一些食材
        r.match_status.available_ingredients.push_back({"鸡蛋", 3.0, "个"});
        r.match_status.available_ingredients.push_back({"番茄", 2.0, "个"});
        r.match_status.missing_ingredients.push_back({"葱花", 1.0, "把"});
        return r;
    }

    PagedRecommendedRecipes makePagedRecommended(int total, int page = 1, int size = 20) {
        PagedRecommendedRecipes r;
        r.pagination = {page, size, total, (total + size - 1) / size};
        r.health_filter_applied = false;
        return r;
    }

    PagedInventory makePagedInventory(int total) {
        PagedInventory r;
        r.pagination = {1, 1, total, total > 0 ? 1 : 0};
        return r;
    }
}

// ==================== 已实现的方法 ====================

TEST(RecipeServiceTest, 公开菜谱列表正确委派) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    auto expected = makePagedRecipes(3);
    EXPECT_CALL(*repo, findPublicRecipes(1, 20, nlohmann::json::object()))
        .WillOnce(Return(expected));

    auto result = service.getPublicRecipes(1, 20, nlohmann::json::object());
    EXPECT_EQ(result.data.size(), 3);
    EXPECT_EQ(result.data[0].name, "Test Recipe");
}

TEST(RecipeServiceTest, 菜谱详情正确委派) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    auto expected = makeDetail(5);
    EXPECT_CALL(*repo, findById(5, testing::_)).WillOnce(Return(expected));

    auto result = service.getRecipeDetail(5);
    EXPECT_EQ(result.id, 5);
    EXPECT_EQ(result.name, "Test Recipe");
}

TEST(RecipeServiceTest, 投稿菜谱正确委派) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    auto expectedResp = SubmitRecipeResponse{99, "pending"};
    auto req = makeSubmitReq();

    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*repo, create(42, Truly([](const auto& r) {
        return r.name == "New Recipe";
    }), _)).WillOnce(Return(expectedResp));

    auto result = service.submitRecipe(42, req);
    EXPECT_EQ(result.id, 99);
    EXPECT_EQ(result.status, "pending");
}

TEST(RecipeServiceTest, 投稿内容完全一致驳回) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    auto req = makeSubmitReq();

    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*repo, create(_, _, _)).Times(0);

    try {
        service.submitRecipe(42, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_THAT(e.what(), testing::HasSubstr("完全一致"));
    }
}

// ==================== 未实现的方法 ====================

TEST(RecipeServiceTest, 搜索菜谱按关键字返回结果) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    auto expected = makePagedRecipes(2);
    expected.data[0].name = "番茄炒蛋";
    expected.data[1].name = "番茄牛腩";
    EXPECT_CALL(*repo, searchRecipes("番茄", 1, 20, _))
        .WillOnce(Return(expected));

    auto result = service.searchRecipes("番茄", 1, 20, {});
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.data[0].name, "番茄炒蛋");
    EXPECT_EQ(result.data[1].name, "番茄牛腩");
}

TEST(RecipeServiceTest, 搜索菜谱无结果返回空列表) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    PagedRecipes emptyResult;
    emptyResult.pagination = {1, 20, 0, 0};
    EXPECT_CALL(*repo, searchRecipes("不存在的菜谱", 1, 20, _))
        .WillOnce(Return(emptyResult));

    auto result = service.searchRecipes("不存在的菜谱", 1, 20, {});
    EXPECT_EQ(result.data.size(), 0);
    EXPECT_EQ(result.pagination.total, 0);
}

// ==================== 智能推荐测试 ====================

TEST(RecipeServiceTest, 推荐库存为空返回400) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* invRepo = mockInv.get();

    // 库存为空
    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(0)));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    try {
        service.getRecommendedRecipes(1, 1, 20);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("库存为空"));
    }
}

TEST(RecipeServiceTest, 推荐正常流程) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    // 库存非空（1件）
    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    // 无偏好
    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Return(UserPreferences{}));

    // 无健康档案
    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Return(std::vector<std::string>{}));

    // 仓库返回 3 个候选
    PagedRecommendedRecipes candidates = makePagedRecommended(3, 1, 60);
    candidates.data.push_back(makeRec(1, "麻辣火锅", "麻辣", "煮", 0.8, 4.5));
    candidates.data.push_back(makeRec(2, "清蒸鱼", "清淡", "蒸", 0.6, 4.0));
    candidates.data.push_back(makeRec(3, "番茄炒蛋", "清淡", "炒", 0.4, 3.5));

    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);

    EXPECT_FALSE(result.health_filter_applied);
    EXPECT_EQ(result.data.size(), 3);
    EXPECT_EQ(result.pagination.total, 3);

    // 应按最终评分降序（麻辣火锅 match_score 最高 + rating 4.5）
    EXPECT_EQ(result.data[0].name, "麻辣火锅");
}

TEST(RecipeServiceTest, 推荐偏好读取异常降级为空偏好) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    // 库存非空（1件）
    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    // 偏好读取抛业务异常（池繁忙 503）→ 降级为空偏好，不向上抛
    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Throw(ServiceException("数据库连接池繁忙", 503)));

    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Return(std::vector<std::string>{}));

    PagedRecommendedRecipes candidates = makePagedRecommended(1, 1, 60);
    candidates.data.push_back(makeRec(1, "清蒸鱼", "清淡", "蒸", 0.6, 4.0));
    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);
    EXPECT_EQ(result.data.size(), 1);
    EXPECT_EQ(result.data[0].name, "清蒸鱼");
}

TEST(RecipeServiceTest, 推荐健康档案读取DB故障降级为无档案) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Return(UserPreferences{}));

    // 健康档案读取抛任意 std::exception → 降级为无健康档案，不向上抛。
    // 注意：真实 PgUserRepository::getHealthConditions 会吞掉非 ServiceException 的
    // std::exception 返回空（见其方法注释），此用例是服务层契约测试——当前装配下
    // 服务层 catch 实际只接得住 ServiceException（如池繁忙 503）。
    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Throw(std::runtime_error("Failed to open database connection")));

    PagedRecommendedRecipes candidates = makePagedRecommended(1, 1, 60);
    candidates.data.push_back(makeRec(1, "清蒸鱼", "清淡", "蒸", 0.6, 4.0));
    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);
    EXPECT_FALSE(result.health_filter_applied);
    EXPECT_EQ(result.data.size(), 1);
}

TEST(RecipeServiceTest, 推荐健康过滤排除禁忌菜谱) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Return(UserPreferences{}));

    // 高血压 → 禁忌 "酱油"
    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Return(std::vector<std::string>{"高血压"}));

    PagedRecommendedRecipes candidates = makePagedRecommended(3, 1, 60);
    auto r1 = makeRec(1, "清蒸鱼", "清淡", "蒸", 0.9, 4.5);
    candidates.data.push_back(r1);

    auto r2 = makeRec(2, "红烧肉", "酱香", "炖", 0.7, 4.0);
    r2.match_status.available_ingredients.push_back({"酱油", 10.0, "毫升"}); // 含酱油
    candidates.data.push_back(r2);

    candidates.data.push_back(makeRec(3, "番茄炒蛋", "清淡", "炒", 0.5, 3.5));

    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);

    // 红烧肉 被排除
    EXPECT_TRUE(result.health_filter_applied);
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.pagination.total, 3); // total 为原始 COUNT，不受健康过滤样本影响
}

TEST(RecipeServiceTest, 推荐偏好厌食减分) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    // 厌恶 "鸡蛋"
    UserPreferences pref;
    pref.dislikes = {"鸡蛋"};
    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Return(pref));

    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Return(std::vector<std::string>{}));

    PagedRecommendedRecipes candidates = makePagedRecommended(2, 1, 60);
    auto r1 = makeRec(1, "蒸蛋羹", "清淡", "蒸", 0.8, 4.0);
    // r1 已有 available_ingredients: 鸡蛋, 番茄 — 命中 dislike
    candidates.data.push_back(r1);

    auto r2 = makeRec(2, "清炒时蔬", "清淡", "炒", 0.6, 4.0);
    r2.match_status.available_ingredients.clear();
    r2.match_status.missing_ingredients.clear();
    r2.match_status.available_ingredients.push_back({"青菜", 1.0, "把"});
    candidates.data.push_back(r2);

    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);

    EXPECT_EQ(result.data.size(), 2);
    // 蒸蛋羹因 dislike 减分，应排在清炒时蔬之后
    EXPECT_EQ(result.data[0].name, "清炒时蔬");
    EXPECT_EQ(result.data[1].name, "蒸蛋羹");
}

TEST(RecipeServiceTest, 推荐喜欢风味与标签独立加分) {
    auto mockRecipe = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto mockUser = std::make_unique<NiceMock<MockUserRepository>>();
    auto mockInv = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* recipeRepo = mockRecipe.get();
    auto* userRepo = mockUser.get();
    auto* invRepo = mockInv.get();

    EXPECT_CALL(*invRepo, findInventory(1, 1, 1))
        .WillOnce(Return(makePagedInventory(1)));

    // 喜欢列表：风味 "清淡" + 标签 "家常"
    UserPreferences pref;
    pref.likes = {"清淡", "家常"};
    EXPECT_CALL(*userRepo, getPreferences(1))
        .WillOnce(Return(pref));

    EXPECT_CALL(*userRepo, getHealthConditions(1))
        .WillOnce(Return(std::vector<std::string>{}));

    // 四道菜：基准完全相同（match 0.63 / 评分 3.0 / 非近期），仅命中维度不同
    PagedRecommendedRecipes candidates = makePagedRecommended(4, 1, 60);
    auto rNone = makeRec(1, "无命中", "甜", "炒", 0.63, 3.0);
    rNone.tags = {"西式"};
    rNone.submitted_at = "2020-01-01T00:00:00";
    candidates.data.push_back(rNone);

    auto rTag = makeRec(2, "仅标签命中", "甜", "炒", 0.63, 3.0);
    rTag.tags = {"家常"};  // 仅命中标签
    rTag.submitted_at = "2020-01-01T00:00:00";
    candidates.data.push_back(rTag);

    auto rFlavor = makeRec(3, "仅风味命中", "清淡", "炒", 0.63, 3.0);
    rFlavor.tags = {"西式"};  // 仅命中风味
    rFlavor.submitted_at = "2020-01-01T00:00:00";
    candidates.data.push_back(rFlavor);

    auto rBoth = makeRec(4, "双命中", "清淡", "炒", 0.63, 3.0);
    rBoth.tags = {"家常"};  // 风味 + 标签同时命中
    rBoth.submitted_at = "2020-01-01T00:00:00";
    candidates.data.push_back(rBoth);

    EXPECT_CALL(*recipeRepo, findRecommendedRecipes(1, 1, 60))
        .WillOnce(Return(candidates));

    RecipeServiceImpl service(std::move(mockRecipe),
                              std::move(mockUser),
                              std::move(mockInv));

    auto result = service.getRecommendedRecipes(1, 1, 20);

    ASSERT_EQ(result.data.size(), 4);
    // 基准分 0.63*0.5 = 0.315；
    // 无命中 → 0.315；仅标签 +0.10*0.25 → 0.34；仅风味 +0.15*0.25 → 0.35；双命中 +0.25*0.25 → 0.38
    EXPECT_EQ(result.data[0].name, "双命中");
    EXPECT_EQ(result.data[0].match_score, 0.38);
    EXPECT_EQ(result.data[1].name, "仅风味命中");
    EXPECT_EQ(result.data[1].match_score, 0.35);
    EXPECT_EQ(result.data[2].name, "仅标签命中");
    EXPECT_EQ(result.data[2].match_score, 0.34);
    EXPECT_EQ(result.data[3].name, "无命中");
    EXPECT_EQ(result.data[3].match_score, 0.32);
}

TEST(RecipeServiceTest, 关联视频查询成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    RecipeDetail recipe = makeDetail(1);
    EXPECT_CALL(*repo, findById(1, testing::_)).WillOnce(Return(recipe));

    std::vector<RecipeVideo> fakeVideos;
    RecipeVideo v1;
    v1.id = 1;
    v1.title = "大厨教你做番茄炒蛋";
    v1.platform = "youtube";
    v1.url = "https://www.youtube.com/watch?v=example";
    v1.thumbnail_url = "https://img.youtube.com/vi/example/hqdefault.jpg";
    v1.duration_seconds = 245;
    fakeVideos.push_back(v1);

    RecipeVideo v2;
    v2.id = 2;
    v2.title = "番茄炒蛋零基础版";
    v2.platform = "bilibili";
    v2.url = "https://www.bilibili.com/video/example";
    v2.duration_seconds = 180;
    fakeVideos.push_back(v2);

    EXPECT_CALL(*repo, findVideos(1)).WillOnce(Return(fakeVideos));

    auto result = service.getRecipeVideos(1);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].id, 1);
    EXPECT_EQ(result[0].title, "大厨教你做番茄炒蛋");
    EXPECT_EQ(result[0].platform, "youtube");
    EXPECT_EQ(result[0].duration_seconds, 245);
    EXPECT_EQ(result[1].id, 2);
    EXPECT_EQ(result[1].title, "番茄炒蛋零基础版");
}

TEST(RecipeServiceTest, 关联视频菜谱不存在) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findById(999, testing::_))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    EXPECT_THROW(service.getRecipeVideos(999), ServiceException);
}

TEST(RecipeServiceTest, 我的投稿列表正确委派) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    PagedMyRecipes expected;
    expected.data = {
        {1, "番茄炒蛋", "approved", std::nullopt, "2026-04-21T10:00:00Z"},
        {2, "红烧肉",   "rejected", "图片不清晰，请重新上传", "2026-04-21T11:00:00Z"}
    };
    expected.pagination = {1, 20, 2, 1};

    EXPECT_CALL(*repo, findMySubmittedRecipes(42, 1, 20, "pending"))
        .WillOnce(Return(expected));

    auto result = service.getMySubmittedRecipes(42, 1, 20, "pending");
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.data[0].name, "番茄炒蛋");
    EXPECT_EQ(result.data[0].status, "approved");
    EXPECT_FALSE(result.data[0].reject_reason.has_value());
    EXPECT_EQ(result.data[1].name, "红烧肉");
    EXPECT_EQ(result.data[1].status, "rejected");
    ASSERT_TRUE(result.data[1].reject_reason.has_value());
    EXPECT_EQ(result.data[1].reject_reason.value(), "图片不清晰，请重新上传");
    EXPECT_EQ(result.data[1].submitted_at, "2026-04-21T11:00:00Z");
    EXPECT_EQ(result.pagination.total, 2);
}

TEST(RecipeServiceTest, 编辑菜谱成功委派) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EditRecipeRequest updates;
    updates.name = "Modified Recipe";
    updates.description = "Modified description";

    EXPECT_CALL(*repo, update(42, 1, ::testing::_, _))
        .WillOnce(Return(std::string("pending")));

    std::string newStatus = service.editRecipe(42, 1, updates);
    EXPECT_EQ(newStatus, "pending");
}

TEST(RecipeServiceTest, 编辑已通过菜谱后进入pending) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, update(42, 1, ::testing::_, _))
        .WillOnce(Return(std::string("pending")));

    std::string newStatus = service.editRecipe(42, 1, {});
    EXPECT_EQ(newStatus, "pending");
}

TEST(RecipeServiceTest, 编辑他人菜谱返回403) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, update(99, 1, ::testing::_, _))
        .WillOnce(Throw(ServiceException("仅可编辑自己投稿的菜谱", 403)));

    try {
        service.editRecipe(99, 1, {});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 403);
        EXPECT_STREQ(e.what(), "仅可编辑自己投稿的菜谱");
    }
}

TEST(RecipeServiceTest, 编辑不存在的菜谱返回404) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, update(42, 999, ::testing::_, _))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    try {
        service.editRecipe(42, 999, {});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "菜谱不存在");
    }
}

TEST(RecipeServiceTest, 切换收藏未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.toggleFavorite(1, 1), ServiceException);
}

TEST(RecipeServiceTest, 我的评分列表查询成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    PagedUserRatings expected;
    UserRatingItem item;
    item.rating_id = 201;
    item.recipe_id = 10;
    item.recipe_name = "番茄炒蛋";
    item.rating = 5;
    item.comment = "简单易做，味道好极了！";
    item.created_at = "2026-04-20T18:30:00Z";
    item.updated_at = "2026-04-21T09:00:00Z";
    expected.data.push_back(item);
    expected.pagination = {1, 20, 1, 1};

    EXPECT_CALL(*repo, findMyRatings(1, 1, 20))
        .WillOnce(Return(expected));

    auto result = service.getMyRatings(1, 1, 20);
    ASSERT_EQ(result.data.size(), 1);
    EXPECT_EQ(result.data[0].rating_id, 201);
    EXPECT_EQ(result.data[0].recipe_id, 10);
    EXPECT_EQ(result.data[0].recipe_name, "番茄炒蛋");
    EXPECT_EQ(result.data[0].rating, 5);
    EXPECT_EQ(result.data[0].comment, "简单易做，味道好极了！");
    EXPECT_EQ(result.data[0].created_at, "2026-04-20T18:30:00Z");
    EXPECT_EQ(result.pagination.total, 1);
}

TEST(RecipeServiceTest, 营养报告查询成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    NutritionReport fake;
    fake.recipe_id = 1;
    fake.recipe_name = "番茄炒蛋";
    fake.per_serving.calories = 350;
    fake.per_serving.protein_g = 15;
    fake.per_serving.fat_g = 20;
    fake.per_serving.carbs_g = 30;
    fake.health_notes = "健康提示";

    NutritionBreakdownItem item;
    item.name = "鸡蛋";
    item.calories = 140;
    item.protein_g = 12;
    item.fat_g = 10;
    item.carbs_g = 2;
    fake.ingredients_breakdown.push_back(item);

    EXPECT_CALL(*repo, findNutrition(1)).WillOnce(Return(fake));

    auto result = service.getRecipeNutrition(1);
    EXPECT_EQ(result.recipe_id, 1);
    EXPECT_EQ(result.recipe_name, "番茄炒蛋");
    EXPECT_EQ(result.per_serving.calories, 350);
    EXPECT_EQ(result.ingredients_breakdown.size(), 1);
    EXPECT_EQ(result.ingredients_breakdown[0].name, "鸡蛋");
}

TEST(RecipeServiceTest, 营养报告菜谱不存在) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findNutrition(999))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    EXPECT_THROW(service.getRecipeNutrition(999), ServiceException);
}

namespace {
    PagedRatings makePagedRatings(int count = 2) {
        PagedRatings result;
        for (int i = 1; i <= count; ++i) {
            RecipeRating r;
            r.id = i;
            r.user_id = 100 + i;
            r.username = "user_" + std::to_string(i);
            r.rating = (i % 5) + 1;
            r.comment = "Comment " + std::to_string(i);
            r.created_at = std::string("2026-04-") + (i < 10 ? "0" : "") + std::to_string(i) + "T10:00:00Z";
            result.data.push_back(std::move(r));
        }
        result.pagination = {1, count, 10, 5};
        return result;
    }
}

TEST(RecipeServiceTest, 获取评分列表成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    RecipeDetail dummy = makeDetail(42);
    EXPECT_CALL(*repo, findById(42, testing::_)).WillOnce(Return(dummy));

    auto fakeRatings = makePagedRatings(3);
    EXPECT_CALL(*repo, findRatings(42, 1, 10)).WillOnce(Return(fakeRatings));

    auto result = service.getRecipeRatings(42, 1, 10);
    ASSERT_EQ(result.data.size(), 3);
    EXPECT_EQ(result.data[0].id, 1);
    EXPECT_EQ(result.data[0].username, "user_1");
    EXPECT_EQ(result.data[0].rating, 2);
    EXPECT_EQ(result.data[0].comment, "Comment 1");
    EXPECT_EQ(result.data[1].username, "user_2");
    EXPECT_EQ(result.data[2].username, "user_3");
    EXPECT_EQ(result.pagination.total, 10);
    EXPECT_EQ(result.pagination.page, 1);
}

TEST(RecipeServiceTest, 获取评分列表菜谱不存在) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findById(999, testing::_))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    EXPECT_THROW(service.getRecipeRatings(999, 1, 10), ServiceException);
}

// ==================== 评分与评论操作 ====================

TEST(RecipeServiceTest, 评分菜谱成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    RateRecipeRequest req{5, "非常好吃"};

    EXPECT_CALL(*repo, findById(42, testing::_)).WillOnce(Return(makeDetail(42)));
    EXPECT_CALL(*repo, rateRecipe(1, 42, Truly([](const auto& r) {
        return r.rating == 5 && r.comment == "非常好吃";
    }))).Times(1);

    service.rateRecipe(1, 42, req);
}

TEST(RecipeServiceTest, 评分菜谱重复提交) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findById(42, testing::_)).WillOnce(Return(makeDetail(42)));
    EXPECT_CALL(*repo, rateRecipe(1, 42, _))
        .WillOnce(Throw(ServiceException("您已评过分", 409)));

    EXPECT_THROW(service.rateRecipe(1, 42, {5, ""}), ServiceException);
}

TEST(RecipeServiceTest, 评分菜谱不存在) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findById(999, testing::_))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    EXPECT_THROW(service.rateRecipe(1, 999, {5, ""}), ServiceException);
}

TEST(RecipeServiceTest, 修改评分成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, updateRating(1, 42, 101, Truly([](const auto& r) {
        return r.rating == 4 && r.comment == "改评";
    }))).Times(1);

    service.updateRating(1, 42, 101, {4, "改评"});
}

TEST(RecipeServiceTest, 修改评分越权) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, updateRating(2, 42, 101, _))
        .WillOnce(Throw(ServiceException("无权限操作他人的评论", 403)));

    EXPECT_THROW(service.updateRating(2, 42, 101, {4, ""}), ServiceException);
}

TEST(RecipeServiceTest, 删除评分成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, deleteRating(1, 42, 101)).Times(1);

    service.deleteRating(1, 42, 101);
}

TEST(RecipeServiceTest, 删除评分越权) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, deleteRating(2, 42, 101))
        .WillOnce(Throw(ServiceException("无权限操作他人的评论", 403)));

    EXPECT_THROW(service.deleteRating(2, 42, 101), ServiceException);
}
