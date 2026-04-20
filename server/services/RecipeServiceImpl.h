#pragma once
#include <gocook/IServices.h>
#include "../DBConnection.h"

class RecipeServiceImpl : public gocook::services::IRecipeService {
public:
    explicit RecipeServiceImpl(DBConnection& db) : db_(db) {}

    // 已实现的方法
    gocook::models::PagedRecipes getPublicRecipes(int page, int size,
                                                  const nlohmann::json& filters) override;

    // 以下方法暂时抛出 Not implemented 异常
    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) override {
        throw std::runtime_error("Not implemented");
    }

    gocook::models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                  int page, int size) override {
        throw std::runtime_error("Not implemented");
    }

    gocook::models::RecipeDetail getRecipeDetail(int recipeId) override {
        throw std::runtime_error("Not implemented");
    }

    std::vector<gocook::models::RecipeVideo> getRecipeVideos(int recipeId) override {
        throw std::runtime_error("Not implemented");
    }

    gocook::models::PagedRatings getRecipeRatings(int recipeId, int page, int size) override {
        throw std::runtime_error("Not implemented");
    }

    gocook::models::SubmitRecipeResponse submitRecipe(int userId,
                                                      const gocook::models::SubmitRecipeRequest& data) override {
        throw std::runtime_error("Not implemented");
    }

    gocook::models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                         int page, int size) override {
        throw std::runtime_error("Not implemented");
    }

    void editRecipe(int userId, int recipeId,
                    const gocook::models::EditRecipeRequest& updates) override {
        throw std::runtime_error("Not implemented");
    }

    void toggleFavorite(int userId, int recipeId) override {
        throw std::runtime_error("Not implemented");
    }

    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& request) override {
        throw std::runtime_error("Not implemented");
    }

private:
    DBConnection& db_;
};