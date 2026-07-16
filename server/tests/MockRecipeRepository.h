#pragma once

#include <gocook/IRecipeRepository.h>
#include <gmock/gmock.h>

class MockRecipeRepository : public gocook::repository::IRecipeRepository {
public:
    MOCK_METHOD(bool, existsByContent,
                (const nlohmann::json&, const nlohmann::json&), (override));
    MOCK_METHOD(gocook::models::PagedRecipes, findPublicRecipes,
                (int, int, const nlohmann::json&), (override));
    MOCK_METHOD(gocook::models::PagedRecipes, searchRecipes,
                (const std::string&, int, int, const nlohmann::json&), (override));
    MOCK_METHOD(gocook::models::PagedRecommendedRecipes, findRecommendedRecipes,
                (int, int, int), (override));
    MOCK_METHOD(gocook::models::RecipeDetail, findById, (int, int), (override));
    MOCK_METHOD(std::vector<gocook::models::RecipeVideo>, findVideos, (int), (override));
    MOCK_METHOD(gocook::models::PagedRatings, findRatings, (int, int, int), (override));
    MOCK_METHOD(gocook::models::SubmitRecipeResponse, create,
                (int, const gocook::models::SubmitRecipeRequest&), (override));
    MOCK_METHOD(gocook::models::PagedMyRecipes, findMySubmittedRecipes,
                (int, int, int, const std::string&), (override));
    MOCK_METHOD(std::string, update,
                (int, int, const gocook::models::EditRecipeRequest&), (override));
    MOCK_METHOD(void, toggleFavorite,
                (int, int, std::optional<int>, std::optional<bool>), (override));
    MOCK_METHOD(void, rateRecipe,
                (int, int, const gocook::models::RateRecipeRequest&), (override));
    MOCK_METHOD(void, updateRating,
                (int, int, int, const gocook::models::RateRecipeRequest&), (override));
    MOCK_METHOD(void, deleteRating, (int, int, int), (override));
    MOCK_METHOD(std::optional<gocook::models::RecipeRating>, findMyRating,
                (int, int), (override));
    MOCK_METHOD(gocook::models::PagedUserRatings, findMyRatings,
                (int, int, int), (override));
    MOCK_METHOD(gocook::models::NutritionReport, findNutrition, (int), (override));
    MOCK_METHOD(std::string, updateRecipeImage, (int, const std::string&), (override));
    MOCK_METHOD(std::string, updateStepImage, (int, int, const std::string&), (override));
    MOCK_METHOD(void, deleteRecipe, (int, int), (override));
};
