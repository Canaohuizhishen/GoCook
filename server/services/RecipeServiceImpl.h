#pragma once
#include <gocook/IServices.h>
#include <gocook/IRecipeRepository.h>
#include <gocook/IUserRepository.h>
#include <gocook/IInventoryRepository.h>
#include <memory>

class RecipeServiceImpl : public gocook::services::IRecipeService {
public:
    explicit RecipeServiceImpl(
        std::unique_ptr<gocook::repository::IRecipeRepository> recipeRepo,
        std::unique_ptr<gocook::repository::IUserRepository> userRepo = nullptr,
        std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo = nullptr)
        : recipeRepo_(std::move(recipeRepo))
        , userRepo_(std::move(userRepo))
        , inventoryRepo_(std::move(inventoryRepo)) {}

    gocook::models::PagedRecipes getPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) override;

    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                                int page, int size,
                                                const nlohmann::json& filters) override;

    gocook::models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                   int page, int size) override;

    gocook::models::RecipeDetail getRecipeDetail(int recipeId) override;

    std::vector<gocook::models::RecipeVideo> getRecipeVideos(int recipeId) override;

    gocook::models::PagedRatings getRecipeRatings(int recipeId, int page, int size) override;

    gocook::models::SubmitRecipeResponse submitRecipe(int userId,
                                                       const gocook::models::SubmitRecipeRequest& data) override;

    gocook::models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                          int page, int size,
                                                          const std::string& status = "") override;

    std::string editRecipe(int userId, int recipeId,
                             const gocook::models::EditRecipeRequest& updates) override;

    void toggleFavorite(int userId, int recipeId,
                        std::optional<int> groupId = std::nullopt,
                        std::optional<bool> isPublic = std::nullopt) override;

    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& request) override;

    void updateRating(int userId, int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& request) override;

    void deleteRating(int userId, int recipeId, int ratingId) override;

    std::optional<gocook::models::RecipeRating> getMyRating(int userId,
                                                              int recipeId) override;

    gocook::models::PagedUserRatings getMyRatings(int userId, int page, int size) override;

    gocook::models::NutritionReport getRecipeNutrition(int recipeId) override;

private:
    std::unique_ptr<gocook::repository::IRecipeRepository> recipeRepo_;
    std::unique_ptr<gocook::repository::IUserRepository> userRepo_;
    std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo_;
};
