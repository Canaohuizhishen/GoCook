#include "RecipeHandler.h"
#include "../auth_middleware.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <vector>
#include "../common/ErrorHelper.h"

using json = nlohmann::json;
using namespace gocook::services;
using namespace gocook::models;

// ========== 辅助序列化函数 ==========

static json toJson(const Pagination& pag) {
    return {
        {"page", pag.page},
        {"size", pag.size},
        {"total", pag.total},
        {"total_pages", pag.total_pages}
    };
}

static json toJson(const RecipeSummary& recipe) {
    json item;
    item["id"] = recipe.id;
    item["name"] = recipe.name;
    item["description"] = recipe.description;
    item["image_url"] = recipe.image_url;
    item["cooking_method"] = recipe.cooking_method;
    item["flavor"] = recipe.flavor;
    item["ingredient_type"] = recipe.ingredient_type;
    item["prep_time_minutes"] = recipe.prep_time_minutes;
    item["cook_time_minutes"] = recipe.cook_time_minutes;
    item["calories"] = recipe.calories;
    item["view_count"] = recipe.view_count;
    item["avg_rating"] = recipe.avg_rating;
    item["tags"] = recipe.tags;
    item["author_id"] = recipe.author_id;
    item["author_name"] = recipe.author_name;
    return item;
}

static json toJson(const PagedRecipes& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const RecommendedRecipe& rec) {
    json item = toJson(static_cast<const RecipeSummary&>(rec));
    item["match_score"] = rec.match_score;
    json matchStatus;
    json available = json::array();
    for (const auto& ing : rec.match_status.available_ingredients) {
        json ingJson;
        ingJson["name"] = ing.name;
        ingJson["quantity"] = ing.quantity;
        ingJson["unit"] = ing.unit;
        available.push_back(ingJson);
    }
    matchStatus["available_ingredients"] = available;
    json missing = json::array();
    for (const auto& ing : rec.match_status.missing_ingredients) {
        json ingJson;
        ingJson["name"] = ing.name;
        ingJson["quantity"] = ing.quantity;
        ingJson["unit"] = ing.unit;
        missing.push_back(ingJson);
    }
    matchStatus["missing_ingredients"] = missing;
    item["match_status"] = matchStatus;
    return item;
}

static json toJson(const PagedRecommendedRecipes& paged) {
    json resp;
    resp["health_filter_applied"] = paged.health_filter_applied;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const RecipeDetail& detail) {
    json item;
    item["id"] = detail.id;
    item["name"] = detail.name;
    item["description"] = detail.description;
    item["image_url"] = detail.image_url;
    item["cooking_method"] = detail.cooking_method;
    item["flavor"] = detail.flavor;
    item["prep_time_minutes"] = detail.prep_time_minutes;
    item["cook_time_minutes"] = detail.cook_time_minutes;
    item["view_count"] = detail.view_count;
    item["avg_rating"] = detail.avg_rating;
    json ingredients = json::array();
    for (const auto& ing : detail.ingredients) {
        json ingJson;
        ingJson["name"] = ing.name;
        ingJson["quantity"] = ing.quantity;
        ingJson["unit"] = ing.unit;
        ingredients.push_back(ingJson);
    }
    item["ingredients"] = ingredients;
    json steps = json::array();
    for (const auto& step : detail.steps) {
        json stepJson;
        stepJson["order"] = step.order;
        stepJson["description"] = step.description;
        if (step.duration.has_value())
            stepJson["duration"] = step.duration.value();
        steps.push_back(stepJson);
    }
    item["steps"] = steps;
    json nutrition;
    nutrition["calories"] = detail.nutrition.calories;
    nutrition["protein"] = detail.nutrition.protein;
    nutrition["fat"] = detail.nutrition.fat;
    nutrition["carbs"] = detail.nutrition.carbs;
    item["nutrition"] = nutrition;
    item["tags"] = detail.tags;
    item["author_id"] = detail.author_id;
    item["author_name"] = detail.author_name;
    item["created_at"] = detail.created_at;
    return item;
}

static json toJson(const RecipeVideo& video) {
    return {
        {"id", video.id},
        {"title", video.title},
        {"platform", video.platform},
        {"url", video.url},
        {"thumbnail_url", video.thumbnail_url},
        {"duration_seconds", video.duration_seconds}
    };
}

static json toJson(const RecipeRating& rating) {
    return {
        {"id", rating.id},
        {"user_id", rating.user_id},
        {"username", rating.username},
        {"rating", rating.rating},
        {"comment", rating.comment},
        {"created_at", rating.created_at}
    };
}

static json toJson(const PagedRatings& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const MyRecipeStatus& status) {
    json item;
    item["id"] = status.id;
    item["name"] = status.name;
    item["status"] = status.status;
    if (status.reject_reason.has_value())
        item["reject_reason"] = status.reject_reason.value();
    item["submitted_at"] = status.submitted_at;
    return item;
}

static json toJson(const PagedMyRecipes& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

// ========== RecipeHandler 实现 ==========

RecipeHandler::RecipeHandler(IRecipeService& service, AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

nlohmann::json RecipeHandler::parseFilterParams(const httplib::Request& req) {
    nlohmann::json filters;
    if (req.has_param("cuisine")) filters["cuisine"] = req.get_param_value("cuisine");
    if (req.has_param("meal_type")) filters["meal_type"] = req.get_param_value("meal_type");
    if (req.has_param("flavor")) filters["flavor"] = req.get_param_value("flavor");
    if (req.has_param("cooking_method")) filters["cooking_method"] = req.get_param_value("cooking_method");
    if (req.has_param("ingredient_type")) filters["ingredient_type"] = req.get_param_value("ingredient_type");
    if (req.has_param("difficulty")) filters["difficulty"] = req.get_param_value("difficulty");
    if (req.has_param("max_time")) filters["max_time"] = std::stoi(req.get_param_value("max_time"));
    if (req.has_param("min_calories")) filters["min_calories"] = std::stoi(req.get_param_value("min_calories"));
    if (req.has_param("max_calories")) filters["max_calories"] = std::stoi(req.get_param_value("max_calories"));
    if (req.has_param("tags")) {
        std::string tagsParam = req.get_param_value("tags");
        std::vector<std::string> tagList;
        std::stringstream ss(tagsParam);
        std::string token;
        while (std::getline(ss, token, ',')) {
            if (!token.empty()) {
                tagList.push_back(token);
            }
        }
        filters["tags"] = tagList;
    }
    if (req.has_param("sort_by")) filters["sort_by"] = req.get_param_value("sort_by");
    if (req.has_param("min_rating")) filters["min_rating"] = std::stod(req.get_param_value("min_rating"));
    return filters;
}

void RecipeHandler::getRecipesPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        auto result = service_.getPublicRecipes(page, size, parseFilterParams(req));
        json response = toJson(result);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::searchRecipes(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string keyword = req.get_param_value("keyword");
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        auto result = service_.searchRecipes(keyword, page, size, parseFilterParams(req));
        json response = toJson(result);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecommendedRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        auto result = service_.getRecommendedRecipes(info.userId, page, size);
        json response = toJson(result);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecipeDetail(const httplib::Request& req, httplib::Response& res) {
    try {
        int recipeId = std::stoi(req.matches[1]);
        auto detail = service_.getRecipeDetail(recipeId);
        json response = toJson(detail);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecipeVideos(const httplib::Request& req, httplib::Response& res) {
    try {
        int recipeId = std::stoi(req.matches[1]);
        auto videos = service_.getRecipeVideos(recipeId);
        json arr = json::array();
        for (const auto& v : videos) arr.push_back(toJson(v));
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = arr.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecipeRatings(const httplib::Request& req, httplib::Response& res) {
    try {
        int recipeId = std::stoi(req.matches[1]);
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 10;
        auto ratings = service_.getRecipeRatings(recipeId, page, size);
        json response = toJson(ratings);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::submitRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        json reqJson = json::parse(req.body);
        SubmitRecipeRequest request;
        request.name = reqJson.value("name", "");
        request.description = reqJson.value("description", "");
        request.image_url = reqJson.value("image_url", "");
        if (reqJson.contains("ingredients") && reqJson["ingredients"].is_array()) {
            for (const auto& ingJson : reqJson["ingredients"]) {
                Ingredient ing;
                ing.name = ingJson.value("name", "");
                ing.quantity = ingJson.value("quantity", 0.0);
                ing.unit = ingJson.value("unit", "");
                request.ingredients.push_back(ing);
            }
        }
        if (reqJson.contains("steps") && reqJson["steps"].is_array()) {
            for (const auto& stepJson : reqJson["steps"]) {
                CookingStep step;
                step.order = stepJson.value("order", 0);
                step.description = stepJson.value("description", "");
                if (stepJson.contains("duration"))
                    step.duration = stepJson["duration"].get<int>();
                request.steps.push_back(step);
            }
        }
        if (reqJson.contains("nutrition")) {
            Nutrition nut;
            nut.calories = reqJson["nutrition"].value("calories", 0.0);
            nut.protein = reqJson["nutrition"].value("protein", 0.0);
            nut.fat = reqJson["nutrition"].value("fat", 0.0);
            nut.carbs = reqJson["nutrition"].value("carbs", 0.0);
            request.nutrition = nut;
        }
        if (reqJson.contains("tags")) request.tags = reqJson["tags"].get<std::vector<std::string>>();
        if (reqJson.contains("cooking_method")) request.cooking_method = reqJson["cooking_method"];
        if (reqJson.contains("flavor")) request.flavor = reqJson["flavor"];
        if (reqJson.contains("ingredient_type")) request.ingredient_type = reqJson["ingredient_type"];

        auto respData = service_.submitRecipe(info.userId, request);
        res.status = 201;
        res.body = json{{"id", respData.id}, {"status", respData.status}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getMySubmittedRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        std::string status = req.get_param_value("status"); // may be empty
        auto result = service_.getMySubmittedRecipes(info.userId, page, size, status);
        json response = toJson(result);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = response.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::editRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int recipeId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        EditRecipeRequest updates;
        if (reqJson.contains("name")) updates.name = reqJson["name"];
        if (reqJson.contains("description")) updates.description = reqJson["description"];
        if (reqJson.contains("image_url")) updates.image_url = reqJson["image_url"];
        // ingredients, steps, nutrition, tags etc. can be partially set
        if (reqJson.contains("ingredients") && reqJson["ingredients"].is_array()) {
            for (const auto& ingJson : reqJson["ingredients"]) {
                Ingredient ing;
                ing.name = ingJson.value("name", "");
                ing.quantity = ingJson.value("quantity", 0.0);
                ing.unit = ingJson.value("unit", "");
                updates.ingredients.push_back(ing);
            }
        }
        if (reqJson.contains("steps") && reqJson["steps"].is_array()) {
            for (const auto& stepJson : reqJson["steps"]) {
                CookingStep step;
                step.order = stepJson.value("order", 0);
                step.description = stepJson.value("description", "");
                if (stepJson.contains("duration"))
                    step.duration = stepJson["duration"].get<int>();
                updates.steps.push_back(step);
            }
        }
        service_.editRecipe(info.userId, recipeId, updates);
        res.status = 200;
        res.body = json{{"message", "Recipe updated"}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::toggleFavorite(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int recipeId = std::stoi(req.matches[1]);
        std::optional<int> groupId = std::nullopt;
        std::optional<bool> isPublic = std::nullopt;
        if (!req.body.empty()) {
            json reqJson = json::parse(req.body);
            if (reqJson.contains("group_id")) groupId = reqJson["group_id"].get<int>();
            if (reqJson.contains("is_public")) isPublic = reqJson["is_public"].get<bool>();
        }
        service_.toggleFavorite(info.userId, recipeId, groupId, isPublic);
        res.status = 200;
        res.body = json{{"message", "Favorite toggled"}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::rateRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int recipeId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        RateRecipeRequest request;
        request.rating = reqJson.at("rating").get<int>();
        request.comment = reqJson.value("comment", "");
        if (request.rating < 1 || request.rating > 5) {
            res.status = 400;
            res.body = json{{"error", "评分必须在1到5之间"}}.dump();
            return;
        }
        service_.rateRecipe(info.userId, recipeId, request);
        res.status = 201;
        res.body = json{{"message", "Rating submitted"}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecipeNutrition(const httplib::Request& req, httplib::Response& res) {
    try {
        int recipeId = std::stoi(req.matches[1]);
        auto report = service_.getRecipeNutrition(recipeId);
        // 序列化 NutritionReport 为 JSON
        json respJson;
        respJson["recipe_id"] = report.recipe_id;
        respJson["recipe_name"] = report.recipe_name;
        respJson["per_serving"]["calories"] = report.per_serving.calories;
        respJson["per_serving"]["protein_g"] = report.per_serving.protein_g;
        respJson["per_serving"]["fat_g"] = report.per_serving.fat_g;
        respJson["per_serving"]["carbs_g"] = report.per_serving.carbs_g;
        respJson["per_serving"]["fiber_g"] = report.per_serving.fiber_g;
        respJson["per_serving"]["sodium_mg"] = report.per_serving.sodium_mg;
        respJson["per_serving"]["vitamin_c_mg"] = report.per_serving.vitamin_c_mg;
        json breakdown = json::array();
        for (const auto& item : report.ingredients_breakdown) {
            json itemJson;
            itemJson["name"] = item.name;
            itemJson["calories"] = item.calories;
            itemJson["protein_g"] = item.protein_g;
            itemJson["fat_g"] = item.fat_g;
            itemJson["carbs_g"] = item.carbs_g;
            breakdown.push_back(itemJson);
        }
        respJson["ingredients_breakdown"] = breakdown;
        respJson["health_notes"] = report.health_notes;
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = respJson.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::updateRating(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int recipeId = std::stoi(req.matches[1]);
        int ratingId = std::stoi(req.matches[2]);
        json reqJson = json::parse(req.body);
        RateRecipeRequest request;
        request.rating = reqJson.value("rating", 0);
        request.comment = reqJson.value("comment", "");
        service_.updateRating(info.userId, recipeId, ratingId, request);
        res.status = 200;
        res.body = json{{"message", "Rating updated"}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::deleteRating(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    try {
        int recipeId = std::stoi(req.matches[1]);
        int ratingId = std::stoi(req.matches[2]);
        service_.deleteRating(info.userId, recipeId, ratingId);
        res.status = 200;
        res.body = json{{"message", "Rating deleted"}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}