#include "RecipeHandler.h"
#include "../auth_utils.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;
using namespace gocook::services;
using namespace gocook::models;

// 辅助：将 PagedRecipes 序列化为 API 规范格式
static json toJson(const PagedRecipes& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& recipe : paged.data) {
        json item;
        item["id"] = recipe.id;
        item["name"] = recipe.name;
        item["description"] = recipe.description;
        item["image_url"] = recipe.image_url;
        item["prep_time_minutes"] = recipe.prep_time_minutes;
        item["cook_time_minutes"] = recipe.cook_time_minutes;
        item["tags"] = recipe.tags;
        item["author_id"] = recipe.author_id;
        item["author_name"] = recipe.author_name;
        resp["data"].push_back(item);
    }
    resp["pagination"] = {
        {"page", paged.pagination.page},
        {"size", paged.pagination.size},
        {"total", paged.pagination.total},
        {"total_pages", paged.pagination.total_pages}
    };
    return resp;
}

RecipeHandler::RecipeHandler(IRecipeService& service) : service_(service) {}

void RecipeHandler::getRecipesPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        nlohmann::json filters;

        auto result = service_.getPublicRecipes(page, size, filters);  // 委托给抽象
        json response = toJson(result);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void RecipeHandler::searchRecipes(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现关键词搜索菜谱（需解析 keyword、分页、筛选参数）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::getRecommendedRecipes(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现智能推荐菜谱（需认证、分页）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::getRecipeDetail(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现获取菜谱详情（需从路径提取 recipeId）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::getRecipeVideos(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现获取菜谱关联视频（需从路径提取 recipeId）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::getRecipeRatings(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现获取评分与评论列表（分页，路径参数 recipeId）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::submitRecipe(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现菜谱投稿（需认证、请求体解析）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::getMySubmittedRecipes(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现获取我的投稿列表（需认证、分页）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::editRecipe(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现编辑未审核菜谱（需认证、路径参数 recipeId、请求体）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::toggleFavorite(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现收藏/取消收藏（需认证、路径参数 recipeId）
    throw gocook::services::ServiceException("Not implemented");
}

void RecipeHandler::rateRecipe(const httplib::Request& req, httplib::Response& res) {
    // TODO: 实现评分与评论（需认证、路径参数 recipeId、请求体）
    throw gocook::services::ServiceException("Not implemented");
}