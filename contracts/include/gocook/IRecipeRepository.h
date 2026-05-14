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

    virtual models::RecipeDetail findById(int recipeId) = 0;

    virtual std::vector<models::RecipeVideo> findVideos(int recipeId) = 0;

    virtual models::PagedRatings findRatings(int recipeId, int page,
                                             int size) = 0;

    virtual models::SubmitRecipeResponse create(
        int userId, const models::SubmitRecipeRequest& data) = 0;

    virtual models::PagedMyRecipes findMySubmittedRecipes(
        int userId, int page, int size, const std::string& status) = 0;

    virtual void update(int userId, int recipeId,
                        const models::EditRecipeRequest& updates) = 0;

    virtual void toggleFavorite(int userId, int recipeId,
                                std::optional<int> groupId,
                                std::optional<bool> isPublic) = 0;

    virtual void rateRecipe(int userId, int recipeId,
                            const models::RateRecipeRequest& req) = 0;

    virtual void updateRating(int userId, int recipeId, int ratingId,
                              const models::RateRecipeRequest& req) = 0;

    virtual void deleteRating(int userId, int recipeId, int ratingId) = 0;

    virtual models::PagedUserRatings findMyRatings(int userId, int page,
                                                   int size) = 0;

    virtual models::NutritionReport findNutrition(int recipeId) = 0;
};

} // namespace gocook::repository
