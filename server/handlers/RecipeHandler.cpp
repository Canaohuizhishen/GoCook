#include "RecipeHandler.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <vector>
#include <fstream>
#include "../common/ErrorHelper.h"
#include "../common/PaginationHelper.h"
#include "../common/JsonSerializer.h"
#include "../common/AuthHelper.h"
#include "../common/Logger.h"

using json = nlohmann::json;
using namespace gocook::services;
using namespace gocook::models;

static std::string extractMime(const std::string& ct) {
    auto p = ct.find(';');
    std::string m = (p == std::string::npos) ? ct : ct.substr(0, p);
    while (!m.empty() && (m.front()==' '||m.front()=='\t')) m.erase(0,1);
    while (!m.empty() && (m.back()==' '||m.back()=='\t')) m.pop_back();
    return m;
}

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
            if (!token.empty()) tagList.push_back(token);
        }
        filters["tags"] = tagList;
    }
    if (req.has_param("sort_by")) filters["sort_by"] = req.get_param_value("sort_by");
    if (req.has_param("min_rating")) filters["min_rating"] = std::stod(req.get_param_value("min_rating"));
    return filters;
}

void RecipeHandler::getRecipesPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        auto pp = parsePagination(req, 20);
        auto result = service_.getPublicRecipes(pp.page, pp.size, parseFilterParams(req));
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::searchRecipes(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string keyword = req.get_param_value("keyword");
        if (keyword.empty()) {
            res.status = 400;
            res.set_header("Content-Type", "application/json");
            res.body = R"({"error":"keyword cannot be empty"})";
            return;
        }
        auto pp = parsePagination(req, 20);
        auto result = service_.searchRecipes(keyword, pp.page, pp.size, parseFilterParams(req));
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecommendedRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto pp = parsePagination(req, 20);
        auto result = service_.getRecommendedRecipes(info.userId, pp.page, pp.size);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getRecipeDetail(const httplib::Request& req, httplib::Response& res) {
    try {
        int recipeId = std::stoi(req.matches[1]);
        // 可选认证：有 token 时查询当前用户收藏状态
        int userId = 0;
        auto tokenInfo = auth_.authenticate(req.get_header_value("Authorization"));
        if (tokenInfo.valid)
            userId = tokenInfo.userId;
        auto detail = service_.getRecipeDetail(recipeId, userId);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(detail).dump();
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
        for (const auto& v : videos) arr.push_back(JsonSerializer::toJson(v));
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
        auto pp = parsePagination(req, 10);
        auto ratings = service_.getRecipeRatings(recipeId, pp.page, pp.size);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(ratings).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::submitRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
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
                step.image_url = stepJson.value("image_url", "");
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
        LOG_ERROR("submitRecipe failed: %s", e.what());
        handleStandardException(e, res);
    }
}

void RecipeHandler::getMySubmittedRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto pp = parsePagination(req, 20);
        std::string status = req.get_param_value("status");
        auto result = service_.getMySubmittedRecipes(info.userId, pp.page, pp.size, status);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::editRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        EditRecipeRequest updates;
        if (reqJson.contains("name")) updates.name = reqJson["name"];
        if (reqJson.contains("description")) updates.description = reqJson["description"];
        if (reqJson.contains("image_url")) updates.image_url = reqJson["image_url"];
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
                step.image_url = stepJson.value("image_url", "");
                if (stepJson.contains("duration"))
                    step.duration = stepJson["duration"].get<int>();
                updates.steps.push_back(step);
            }
        }
        if (reqJson.contains("nutrition")) {
            Nutrition nut;
            nut.calories = reqJson["nutrition"].value("calories", 0.0);
            nut.protein = reqJson["nutrition"].value("protein", 0.0);
            nut.fat = reqJson["nutrition"].value("fat", 0.0);
            nut.carbs = reqJson["nutrition"].value("carbs", 0.0);
            updates.nutrition = nut;
        }
        if (reqJson.contains("tags")) updates.tags = reqJson["tags"].get<std::vector<std::string>>();
        if (reqJson.contains("cooking_method")) updates.cooking_method = reqJson["cooking_method"];
        if (reqJson.contains("flavor")) updates.flavor = reqJson["flavor"];
        if (reqJson.contains("ingredient_type")) updates.ingredient_type = reqJson["ingredient_type"];

        std::string newStatus = service_.editRecipe(info.userId, recipeId, updates);
        res.status = 200;
        res.body = json{{"message", "Recipe updated"}, {"status", newStatus}}.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::toggleFavorite(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
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
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
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
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(report).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getMyRecipeRating(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        auto rating = service_.getMyRating(info.userId, recipeId);
        if (!rating.has_value()) {
            res.status = 404;
            res.set_header("Content-Type", "application/json");
            res.body = json{{"error", "未找到评分"}}.dump();
            return;
        }
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(rating.value()).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::getMyRatings(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto pp = parsePagination(req, 20);
        auto result = service_.getMyRatings(info.userId, pp.page, pp.size);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::updateRating(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
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
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
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

void RecipeHandler::uploadRecipeImage(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        const std::string& content = req.body;

        if (content.empty()) {
            setErrorResponse(res, 400, "请选择 JPG 或 PNG 格式的图片");
            return;
        }

        std::string contentType = extractMime(req.get_header_value("Content-Type"));
        if (contentType != "image/jpeg" && contentType != "image/png"
            && contentType != "image/jpg" && contentType != "image/gif"
            && contentType != "image/bmp" && contentType != "image/svg+xml") {
            setErrorResponse(res, 400, "不支持的图片格式，请使用 JPG/PNG/GIF/BMP/SVG");
            return;
        }

        if (content.size() > 5 * 1024 * 1024) {
            setErrorResponse(res, 400, "图片大小不能超过5MB");
            return;
        }

        std::string ext = ".jpg";
        if (contentType == "image/png")          ext = ".png";
        else if (contentType == "image/gif")      ext = ".gif";
        else if (contentType == "image/bmp")      ext = ".bmp";
        else if (contentType == "image/svg+xml")  ext = ".svg";
        std::string tempPath = "/tmp/gocook_recipe_" + std::to_string(recipeId)
                             + "_" + std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()) + ext;

        {
            std::ofstream ofs(tempPath, std::ios::binary);
            if (!ofs) {
                setErrorResponse(res, 500, "文件写入失败");
                return;
            }
            ofs.write(content.data(), content.size());
            ofs.close();
        }

        auto imageUrl = service_.uploadRecipeImage(recipeId, tempPath);

        res.status = 200;
        res.set_header("Content-Type", "application/json");
        res.body = json{
            {"image_url", imageUrl}
        }.dump();

    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::uploadStepImage(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        int stepIndex = std::stoi(req.matches[2]);
        const std::string& content = req.body;

        if (content.empty()) {
            setErrorResponse(res, 400, "请选择图片");
            return;
        }

        std::string contentType = extractMime(req.get_header_value("Content-Type"));
        if (contentType != "image/jpeg" && contentType != "image/png"
            && contentType != "image/jpg" && contentType != "image/gif"
            && contentType != "image/bmp" && contentType != "image/svg+xml") {
            setErrorResponse(res, 400, "不支持的图片格式");
            return;
        }

        if (content.size() > 5 * 1024 * 1024) {
            setErrorResponse(res, 400, "图片大小不能超过5MB");
            return;
        }

        std::string ext = ".jpg";
        if (contentType == "image/png")          ext = ".png";
        else if (contentType == "image/gif")      ext = ".gif";
        else if (contentType == "image/bmp")      ext = ".bmp";
        else if (contentType == "image/svg+xml")  ext = ".svg";
        std::string tempPath = "/tmp/gocook_recipe_" + std::to_string(recipeId)
                             + "_step_" + std::to_string(stepIndex)
                             + "_" + std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()) + ext;

        {
            std::ofstream ofs(tempPath, std::ios::binary);
            if (!ofs) {
                setErrorResponse(res, 500, "文件写入失败");
                return;
            }
            ofs.write(content.data(), content.size());
            ofs.close();
        }

        std::string imageUrl = service_.uploadStepImage(recipeId, stepIndex, tempPath);

        res.status = 200;
        res.set_header("Content-Type", "application/json");
        res.body = json{ {"image_url", imageUrl}, {"message", "Step image uploaded"} }.dump();

    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void RecipeHandler::deleteRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        service_.deleteRecipe(info.userId, recipeId);
        res.status = 200;
        res.set_header("Content-Type", "application/json");
        res.body = json{ {"message", "Recipe deleted"} }.dump();
    } catch (const ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}
