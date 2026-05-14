#pragma once
#include <gocook/IServices.h>
#include "../ConnectionPool.h"

class RecipeServiceImpl : public gocook::services::IRecipeService {
public:
    explicit RecipeServiceImpl(ConnectionPool& db) : db_(db) {}

    // 已实现的方法
    gocook::models::PagedRecipes getPublicRecipes(int page, int size,
                                                  const nlohmann::json& filters) override;

    // 以下方法暂时抛出 Not implemented 异常
    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                  int page, int size) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::RecipeDetail getRecipeDetail(int recipeId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    std::vector<gocook::models::RecipeVideo> getRecipeVideos(int recipeId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::PagedRatings getRecipeRatings(int recipeId, int page, int size) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::SubmitRecipeResponse submitRecipe(int userId,
                                                      const gocook::models::SubmitRecipeRequest& data) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                         int page, int size,
                                                         const std::string& status = "") override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    void editRecipe(int userId, int recipeId,
                    const gocook::models::EditRecipeRequest& updates) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    void toggleFavorite(int userId, int recipeId,
                        std::optional<int> groupId = std::nullopt,
                        std::optional<bool> isPublic = std::nullopt) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    void updateRating(int userId, int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    void deleteRating(int userId, int recipeId, int ratingId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::PagedUserRatings getMyRatings(int userId, int page, int size) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

    gocook::models::NutritionReport getRecipeNutrition(int recipeId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

private:
    ConnectionPool& db_;
};