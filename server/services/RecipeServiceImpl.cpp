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
std::vector<RecipeVideo> RecipeServiceImpl::getRecipeVideos(int recipeId) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    return recipeRepo_->findVideos(recipeId);
}
PagedRatings RecipeServiceImpl::getRecipeRatings(int recipeId, int page, int size) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    return recipeRepo_->findRatings(recipeId, page, size);
}
PagedMyRecipes RecipeServiceImpl::getMySubmittedRecipes(int userId, int page, int size, const std::string& status) {
    return recipeRepo_->findMySubmittedRecipes(userId, page, size, status);
}
void RecipeServiceImpl::editRecipe(int, int, const EditRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::toggleFavorite(int, int, std::optional<int>, std::optional<bool>) {
    throw ServiceException("Not implemented", 501);
}
void RecipeServiceImpl::rateRecipe(int userId, int recipeId,
                                   const RateRecipeRequest& request) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    recipeRepo_->rateRecipe(userId, recipeId, request);
}
void RecipeServiceImpl::updateRating(int userId, int recipeId, int ratingId,
                                     const RateRecipeRequest& request) {
    recipeRepo_->updateRating(userId, recipeId, ratingId, request);
}
void RecipeServiceImpl::deleteRating(int userId, int recipeId, int ratingId) {
    recipeRepo_->deleteRating(userId, recipeId, ratingId);
}

std::optional<RecipeRating> RecipeServiceImpl::getMyRating(int userId, int recipeId) {
    return recipeRepo_->findMyRating(userId, recipeId);
}

PagedUserRatings RecipeServiceImpl::getMyRatings(int, int, int) {
    throw ServiceException("Not implemented", 501);
}
NutritionReport RecipeServiceImpl::getRecipeNutrition(int recipeId) {
    return recipeRepo_->findNutrition(recipeId);
}
