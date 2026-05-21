#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../services/RecipeServiceImpl.h"
#include "MockRecipeRepository.h"

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
                "炒", "清淡", 10, 20, 100, 4.5, {}, {}, {}, {"test"}, 1, "Chef", "2026-01-01"};
    }

    SubmitRecipeRequest makeSubmitReq() {
        return {"New Recipe", "Yummy", "img.jpg", {}, {}, std::nullopt, {}, std::nullopt, std::nullopt, std::nullopt};
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
    EXPECT_CALL(*repo, findById(5)).WillOnce(Return(expected));

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

    EXPECT_CALL(*repo, create(42, Truly([](const auto& r) {
        return r.name == "New Recipe";
    }))).WillOnce(Return(expectedResp));

    auto result = service.submitRecipe(42, req);
    EXPECT_EQ(result.id, 99);
    EXPECT_EQ(result.status, "pending");
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

TEST(RecipeServiceTest, 智能推荐未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getRecommendedRecipes(1, 1, 20), ServiceException);
}

TEST(RecipeServiceTest, 关联视频查询成功) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    RecipeDetail recipe = makeDetail(1);
    EXPECT_CALL(*repo, findById(1)).WillOnce(Return(recipe));

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

    EXPECT_CALL(*repo, findById(999))
        .WillOnce(Throw(ServiceException("菜谱不存在", 404)));

    EXPECT_THROW(service.getRecipeVideos(999), ServiceException);
}

TEST(RecipeServiceTest, 评分评论未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getRecipeRatings(1, 1, 20), ServiceException);
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

TEST(RecipeServiceTest, 编辑菜谱未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.editRecipe(1, 1, {}), ServiceException);
}

TEST(RecipeServiceTest, 切换收藏未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.toggleFavorite(1, 1), ServiceException);
}

TEST(RecipeServiceTest, 评分菜谱未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.rateRecipe(1, 1, {}), ServiceException);
}

TEST(RecipeServiceTest, 修改评分未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.updateRating(1, 1, 1, {}), ServiceException);
}

TEST(RecipeServiceTest, 删除评分未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.deleteRating(1, 1, 1), ServiceException);
}

TEST(RecipeServiceTest, 我的评分列表未实现) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    RecipeServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getMyRatings(1, 1, 20), ServiceException);
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
