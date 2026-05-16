#include "RecipeServiceImpl.h"

using namespace gocook::models;
using namespace gocook::services;

PagedRecipes RecipeServiceImpl::getPublicRecipes(int page, int size,
                                                  const nlohmann::json& filters) {
    return recipeRepo_->findPublicRecipes(page, size, filters);
}

// 以下方法暂时未实现（骨架）
PagedRecipes RecipeServiceImpl::searchRecipes(const std::string& keyword,
                                                int page, int size,
                                                const nlohmann::json& filters) {
    return recipeRepo_->searchRecipes(keyword, page, size, filters);
}
PagedRecommendedRecipes RecipeServiceImpl::getRecommendedRecipes(int, int, int) {
    throw ServiceException("Not implemented", 501);
}
RecipeDetail RecipeServiceImpl::getRecipeDetail(int recipeId) {
    return recipeRepo_->findById(recipeId);
}

SubmitRecipeResponse RecipeServiceImpl::submitRecipe(int userId, const SubmitRecipeRequest& data) {
    return recipeRepo_->create(userId, data);
}
std::vector<RecipeVideo> RecipeServiceImpl::getRecipeVideos(int) {
    throw ServiceException("Not implemented", 501);
}
PagedRatings RecipeServiceImpl::getRecipeRatings(int, int, int) {
    throw ServiceException("Not implemented", 501);
}
PagedMyRecipes RecipeServiceImpl::getMySubmittedRecipes(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::editRecipe(int, int, const EditRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::toggleFavorite(int, int, std::optional<int>, std::optional<bool>) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::rateRecipe(int, int, const RateRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::updateRating(int, int, int, const RateRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::deleteRating(int, int, int) {
    throw ServiceException("Not implemented", 501);
}
PagedUserRatings RecipeServiceImpl::getMyRatings(int, int, int) {
    throw ServiceException("Not implemented", 501);
}
NutritionReport RecipeServiceImpl::getRecipeNutrition(int) {
    throw ServiceException("Not implemented", 501);
}
