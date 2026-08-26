#pragma once

#include <gocook/IRecipeRepository.h>
#include <gmock/gmock.h>

/**
 * @brief IRecipeRepository 的 Google Mock 替身，用于服务层单元测试。
 *
 * 在测试中通过 EXPECT_CALL 为每个方法设置预期调用与返回值，
 * 方法契约见 IRecipeRepository.h 接口文档。
 */
class MockRecipeRepository : public gocook::repository::IRecipeRepository {
public:
    // 内容查重：检查食材与步骤完全一致的已审核菜谱
    MOCK_METHOD(bool, existsByContent,
                (const nlohmann::json&, const nlohmann::json&), (override));
    // 查询公开菜谱列表
    MOCK_METHOD(gocook::models::PagedRecipes, findPublicRecipes,
                (int, int, const nlohmann::json&), (override));
    // 关键词搜索菜谱
    MOCK_METHOD(gocook::models::PagedRecipes, searchRecipes,
                (const std::string&, int, int, const nlohmann::json&), (override));
    // 查询智能推荐菜谱
    MOCK_METHOD(gocook::models::PagedRecommendedRecipes, findRecommendedRecipes,
                (int, int, int), (override));
    // 按 ID 查询菜谱详情
    MOCK_METHOD(gocook::models::RecipeDetail, findById, (int, int), (override));
    // 查询菜谱关联视频列表
    MOCK_METHOD(std::vector<gocook::models::RecipeVideo>, findVideos, (int), (override));
    // 查询菜谱评分与评论
    MOCK_METHOD(gocook::models::PagedRatings, findRatings, (int, int, int), (override));
    // 创建菜谱投稿（第三个参数为服务层计算好的营养 JSON）
    MOCK_METHOD(gocook::models::SubmitRecipeResponse, create,
                (int, const gocook::models::SubmitRecipeRequest&, const nlohmann::json&), (override));
    // 查询当前用户的投稿列表
    MOCK_METHOD(gocook::models::PagedMyRecipes, findMySubmittedRecipes,
                (int, int, int, const std::string&), (override));
    // 更新未审核菜谱（第四个参数为服务层计算好的营养 JSON；nullopt=保留旧营养）
    MOCK_METHOD(std::string, update,
                (int, int, const gocook::models::EditRecipeRequest&, const std::optional<nlohmann::json>&), (override));
    // 切换收藏状态
    MOCK_METHOD(void, toggleFavorite,
                (int, int, std::optional<int>, std::optional<bool>), (override));
    // 评分与评论
    MOCK_METHOD(void, rateRecipe,
                (int, int, const gocook::models::RateRecipeRequest&), (override));
    // 修改评论
    MOCK_METHOD(void, updateRating,
                (int, int, int, const gocook::models::RateRecipeRequest&), (override));
    // 删除评论
    MOCK_METHOD(void, deleteRating, (int, int, int), (override));
    // 查询当前用户对某菜谱的评分
    MOCK_METHOD(std::optional<gocook::models::RecipeRating>, findMyRating,
                (int, int), (override));
    // 查询当前用户的所有评论列表
    MOCK_METHOD(gocook::models::PagedUserRatings, findMyRatings,
                (int, int, int), (override));
    // 查询独立营养报告
    MOCK_METHOD(gocook::models::NutritionReport, findNutrition, (int), (override));
    // 更新菜谱封面图片
    MOCK_METHOD(std::string, updateRecipeImage, (int, const std::string&), (override));
    // 更新菜谱步骤图片
    MOCK_METHOD(std::string, updateStepImage, (int, int, const std::string&), (override));
    // 删除待审核菜谱
    MOCK_METHOD(void, deleteRecipe, (int, int), (override));
};
