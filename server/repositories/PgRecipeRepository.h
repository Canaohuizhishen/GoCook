#pragma once

#include <gocook/IRecipeRepository.h>
#include "../common/ConnectionPool.h"

class PgRecipeRepository : public gocook::repository::IRecipeRepository {
public:
    explicit PgRecipeRepository(ConnectionPool& db) : db_(db) {}

    gocook::models::PagedRecipes findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) override;

    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) override;

    gocook::models::PagedRecommendedRecipes findRecommendedRecipes(int userId,
                                                                    int page,
                                                                    int size) override;

    gocook::models::RecipeDetail findById(int recipeId) override;

    std::vector<gocook::models::RecipeVideo> findVideos(int recipeId) override;

    gocook::models::PagedRatings findRatings(int recipeId, int page,
                                             int size) override;

    gocook::models::SubmitRecipeResponse create(
        int userId, const gocook::models::SubmitRecipeRequest& data) override;

    gocook::models::PagedMyRecipes findMySubmittedRecipes(
        int userId, int page, int size, const std::string& status) override;

    void update(int userId, int recipeId,
                const gocook::models::EditRecipeRequest& updates) override;

    void toggleFavorite(int userId, int recipeId,
                        std::optional<int> groupId,
                        std::optional<bool> isPublic) override;

    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& req) override;

    void updateRating(int userId, int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& req) override;

    void deleteRating(int userId, int recipeId, int ratingId) override;

    std::optional<gocook::models::RecipeRating> findMyRating(
        int userId, int recipeId) override;

    gocook::models::PagedUserRatings findMyRatings(int userId, int page,
                                                   int size) override;

    gocook::models::NutritionReport findNutrition(int recipeId) override;

private:
    ConnectionPool& db_;
};
