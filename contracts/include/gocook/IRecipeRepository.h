#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>

namespace gocook::repository {

class IRecipeRepository {
public:
    virtual ~IRecipeRepository() = default;

    virtual models::PagedRecipes findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) = 0;

    virtual models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) = 0;

    virtual models::PagedRecommendedRecipes findRecommendedRecipes(int userId,
                                                                    int page,
                                                                    int size) = 0;

    virtual models::RecipeDetail findById(int recipeId, int userId = 0) = 0;

    virtual std::vector<models::RecipeVideo> findVideos(int recipeId) = 0;

    virtual models::PagedRatings findRatings(int recipeId, int page,
                                             int size) = 0;

    virtual models::SubmitRecipeResponse create(
        int userId, const models::SubmitRecipeRequest& data) = 0;

    virtual models::PagedMyRecipes findMySubmittedRecipes(
        int userId, int page, int size, const std::string& status) = 0;

    virtual std::string update(int userId, int recipeId,
                                const models::EditRecipeRequest& updates) = 0;

    virtual void toggleFavorite(int userId, int recipeId,
                                std::optional<int> groupId,
                                std::optional<bool> isPublic) = 0;

    virtual void rateRecipe(int userId, int recipeId,
                            const models::RateRecipeRequest& req) = 0;

    virtual void updateRating(int userId, int recipeId, int ratingId,
                              const models::RateRecipeRequest& req) = 0;

    virtual void deleteRating(int userId, int recipeId, int ratingId) = 0;

    virtual std::optional<models::RecipeRating> findMyRating(
        int userId, int recipeId) = 0;

    virtual models::PagedUserRatings findMyRatings(int userId, int page,
                                                   int size) = 0;

    virtual models::NutritionReport findNutrition(int recipeId) = 0;

    /// 更新菜谱封面图片：将 imagePath 文件复制到 uploads 目录，返回可访问的 image_url
    virtual std::string updateRecipeImage(int recipeId,
                                          const std::string& imagePath) = 0;

    /// 更新菜谱某一步骤的图片：读 steps JSONB → 改 [stepIndex].image_url → 写回，返回 image_url
    virtual std::string updateStepImage(int recipeId, int stepIndex,
                                        const std::string& imagePath) = 0;

    /// 删除待审核菜谱（仅 status != 'approved' 的菜谱可删除），同时清理图片文件
    virtual void deleteRecipe(int userId, int recipeId) = 0;
};

} // namespace gocook::repository
