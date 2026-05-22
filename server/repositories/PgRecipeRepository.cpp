#include "PgRecipeRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

namespace {

struct ParamBuilder {
    std::vector<std::string> values;

    std::string next() {
        return "$" + std::to_string(values.size() + 1);
    }

    void add(const std::string& val) {
        values.push_back(val);
    }

    void addInt(int val) {
        values.push_back(std::to_string(val));
    }
};

std::vector<std::string> parseTags(const pqxx::field& field) {
    if (field.is_null()) return {};
    auto arr = json::parse(field.c_str());
    std::vector<std::string> tags;
    for (const auto& t : arr)
        tags.push_back(t.get<std::string>());
    return tags;
}

void applyRecipeFilters(const nlohmann::json& filters,
                        std::string& where,
                        ParamBuilder& pb) {
    auto addTag = [&](const std::string& v) {
        where += " AND " + pb.next() + " = ANY(r.tags)";
        pb.add(v);
    };

    if (filters.contains("cuisine"))         addTag(filters["cuisine"].get<std::string>());
    if (filters.contains("meal_type"))        addTag(filters["meal_type"].get<std::string>());
    if (filters.contains("difficulty"))       addTag(filters["difficulty"].get<std::string>());
    if (filters.contains("flavor")) {
        where += " AND r.flavor = " + pb.next();
        pb.add(filters["flavor"].get<std::string>());
    }
    if (filters.contains("cooking_method")) {
        where += " AND r.cooking_method = " + pb.next();
        pb.add(filters["cooking_method"].get<std::string>());
    }
    if (filters.contains("ingredient_type")) {
        where += " AND r.ingredient_type = " + pb.next();
        pb.add(filters["ingredient_type"].get<std::string>());
    }
    if (filters.contains("max_time")) {
        where += " AND (r.prep_time_minutes + r.cook_time_minutes) <= " + pb.next();
        pb.addInt(filters["max_time"].get<int>());
    }
    if (filters.contains("min_calories")) {
        where += " AND (r.nutrition_info->>'calories')::numeric >= " + pb.next();
        pb.addInt(filters["min_calories"].get<int>());
    }
    if (filters.contains("max_calories")) {
        where += " AND (r.nutrition_info->>'calories')::numeric <= " + pb.next();
        pb.addInt(filters["max_calories"].get<int>());
    }
    if (filters.contains("min_rating")) {
        where += " AND r.avg_rating >= " + pb.next();
        pb.add(std::to_string(filters["min_rating"].get<double>()));
    }
    if (filters.contains("tags")) {
        for (const auto& t : filters["tags"])
            addTag(t.get<std::string>());
    }
}

} // anonymous namespace

PagedRecipes PgRecipeRepository::findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) {
    PagedRecipes result;
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        int offset = (page > 0) ? (page - 1) * size : 0;

        std::string where = "WHERE 1=1";
        ParamBuilder pb;

        applyRecipeFilters(filters, where, pb);

        std::string countSql = "SELECT COUNT(*) FROM recipes r " + where;
        int total;
        if (pb.values.empty()) {
            total = txn.exec(countSql)[0][0].as<int>();
        } else {
            total = txn.exec_params(countSql, pqxx::prepare::make_dynamic_params(pb.values))
                        [0][0].as<int>();
        }

        std::string order = "ORDER BY r.created_at DESC";
        if (filters.contains("sort_by")) {
            std::string sort = filters["sort_by"].get<std::string>();
            if (sort == "popular") order = "ORDER BY r.view_count DESC";
            else if (sort == "rating") order = "ORDER BY r.avg_rating DESC";
            else if (sort == "newest") order = "ORDER BY r.created_at DESC";
        }

        where += " " + order + " LIMIT " + pb.next();
        pb.addInt(size);
        where += " OFFSET " + pb.next();
        pb.addInt(offset);

        std::string dataSql = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url,
                   array_to_json(r.tags) AS tags_json,
                   COALESCE((r.nutrition_info->>'calories')::numeric, 0) AS calories,
                   r.author_id,
                   u.username AS author_name, r.cooking_method, r.flavor,
                   r.ingredient_type, r.view_count, r.avg_rating
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
        )" + where;

        auto rows = txn.exec_params(dataSql, pqxx::prepare::make_dynamic_params(pb.values));

        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>(0);
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>(0);
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            recipe.tags = parseTags(row["tags_json"]);
            recipe.calories = row["calories"].as<int>(0);

            recipe.author_id = row["author_id"].as<int>(0);
            if (!row["author_name"].is_null())
                recipe.author_name = row["author_name"].c_str();
            else
                recipe.author_name = "unknown";

            if (!row["cooking_method"].is_null())
                recipe.cooking_method = row["cooking_method"].c_str();
            if (!row["flavor"].is_null())
                recipe.flavor = row["flavor"].c_str();
            if (!row["ingredient_type"].is_null())
                recipe.ingredient_type = row["ingredient_type"].c_str();
            recipe.view_count = row["view_count"].as<int>(0);
            recipe.avg_rating = row["avg_rating"].as<double>(0.0);

            result.data.push_back(recipe);
        }

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = (total + size - 1) / size;

        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in findPublicRecipes: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
    return result;
}

PagedRecipes PgRecipeRepository::searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) {
    PagedRecipes result;
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        int offset = (page > 0) ? (page - 1) * size : 0;

        std::string where = "WHERE 1=1";
        ParamBuilder pb;

        // 关键词搜索：匹配菜名、描述、食材名称（ingredients 是 JSONB 数组）
        if (!keyword.empty()) {
            std::string safeKw = txn.esc(keyword);
            where += " AND (r.name ILIKE '%' || '" + safeKw + "' || '%'"
                     " OR r.description ILIKE '%' || '" + safeKw + "' || '%'"
                     " OR EXISTS (SELECT 1 FROM jsonb_array_elements(r.ingredients) AS ing"
                     "           WHERE ing->>'name' ILIKE '%' || '" + safeKw + "' || '%'))";
        }

        applyRecipeFilters(filters, where, pb);

        std::string countSql = "SELECT COUNT(*) FROM recipes r " + where;
        int total;
        if (pb.values.empty()) {
            total = txn.exec(countSql)[0][0].as<int>();
        } else {
            total = txn.exec_params(countSql, pqxx::prepare::make_dynamic_params(pb.values))
                        [0][0].as<int>();
        }

        std::string order = "ORDER BY r.created_at DESC";
        if (filters.contains("sort_by")) {
            std::string sort = filters["sort_by"].get<std::string>();
            if (sort == "popular") order = "ORDER BY r.view_count DESC";
            else if (sort == "rating") order = "ORDER BY r.avg_rating DESC";
            else if (sort == "newest") order = "ORDER BY r.created_at DESC";
        }

        where += " " + order + " LIMIT " + pb.next();
        pb.addInt(size);
        where += " OFFSET " + pb.next();
        pb.addInt(offset);

        std::string dataSql = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url,
                   array_to_json(r.tags) AS tags_json,
                   COALESCE((r.nutrition_info->>'calories')::numeric, 0) AS calories,
                   r.author_id,
                   u.username AS author_name, r.cooking_method, r.flavor,
                   r.ingredient_type, r.view_count, r.avg_rating
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
        )" + where;

        auto rows = txn.exec_params(dataSql, pqxx::prepare::make_dynamic_params(pb.values));

        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>();
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>();
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            recipe.tags = parseTags(row["tags_json"]);
            recipe.calories = row["calories"].as<int>(0);

            recipe.author_id = row["author_id"].as<int>(0);
            if (!row["author_name"].is_null())
                recipe.author_name = row["author_name"].c_str();
            else
                recipe.author_name = "unknown";

            if (!row["cooking_method"].is_null())
                recipe.cooking_method = row["cooking_method"].c_str();
            if (!row["flavor"].is_null())
                recipe.flavor = row["flavor"].c_str();
            if (!row["ingredient_type"].is_null())
                recipe.ingredient_type = row["ingredient_type"].c_str();
            recipe.view_count = row["view_count"].as<int>(0);
            recipe.avg_rating = row["avg_rating"].as<double>(0.0);

            result.data.push_back(recipe);
        }

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = (total + size - 1) / size;

        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in searchRecipes: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
    return result;
}

PagedRecommendedRecipes PgRecipeRepository::findRecommendedRecipes(int, int, int) {
    throw ServiceException("Not implemented", 501);
}

RecipeDetail PgRecipeRepository::findById(int recipeId, int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        bool checkFav = (userId > 0);
        std::string favSelect = checkFav
            ? ", CASE WHEN f.id IS NOT NULL THEN true ELSE false END AS is_favorited"
            : "";
        std::string favJoin = checkFav
            ? " LEFT JOIN favorites f ON f.recipe_id = r.id AND f.user_id = $2"
            : "";
        std::string sql = "SELECT r.id, r.name, r.description, r.image_url,"
            " r.cooking_method, r.flavor,"
            " r.prep_time_minutes, r.cook_time_minutes,"
            " r.view_count, r.avg_rating,"
            " r.ingredients, r.steps, r.nutrition_info,"
            " array_to_json(r.tags) AS tags_json,"
            " r.author_id, u.username AS author_name,"
            " r.created_at"
            + favSelect
            + " FROM recipes r"
            + " LEFT JOIN users u ON r.author_id = u.id"
            + favJoin
            + " WHERE r.id = $1";

        pqxx::result r;
        if (checkFav)
            r = txn.exec_params(sql, recipeId, userId);
        else
            r = txn.exec_params(sql, recipeId);

        if (r.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        const auto& row = r[0];

        RecipeDetail detail;
        detail.id = row["id"].as<int>();
        detail.name = row["name"].c_str();
        detail.description = row["description"].as<std::string>("");
        detail.image_url = row["image_url"].as<std::string>("");
        detail.cooking_method = row["cooking_method"].as<std::string>("");
        detail.flavor = row["flavor"].as<std::string>("");
        detail.prep_time_minutes = row["prep_time_minutes"].as<int>(0);
        detail.cook_time_minutes = row["cook_time_minutes"].as<int>(0);
        detail.view_count = row["view_count"].as<int>(0);
        detail.avg_rating = row["avg_rating"].as<double>(0.0);
        detail.is_favorited = checkFav ? row["is_favorited"].as<bool>(false) : false;

        if (!row["ingredients"].is_null()) {
            auto ingredientsArr = json::parse(row["ingredients"].c_str());
            for (const auto& ing : ingredientsArr) {
                Ingredient ingredient;
                ingredient.name = ing.value("name", "");
                ingredient.quantity = ing.value("quantity", 0.0);
                ingredient.unit = ing.value("unit", "");
                detail.ingredients.push_back(ingredient);
            }
        }

        if (!row["steps"].is_null()) {
            auto stepsArr = json::parse(row["steps"].c_str());
            for (const auto& s : stepsArr) {
                CookingStep step;
                step.order = s.value("order", 0);
                step.description = s.value("description", "");
                if (s.contains("duration"))
                    step.duration = s["duration"].get<int>();
                detail.steps.push_back(step);
            }
        }

        if (!row["nutrition_info"].is_null()) {
            auto nutJson = json::parse(row["nutrition_info"].c_str());
            detail.nutrition.calories = nutJson.value("calories", 0.0);
            detail.nutrition.protein = nutJson.value("protein", 0.0);
            detail.nutrition.fat = nutJson.value("fat", 0.0);
            detail.nutrition.carbs = nutJson.value("carbs", 0.0);
        }

        detail.tags = parseTags(row["tags_json"]);

        detail.author_id = row["author_id"].as<int>(0);
        if (!row["author_name"].is_null())
            detail.author_name = row["author_name"].c_str();
        else
            detail.author_name = "unknown";

        detail.created_at = row["created_at"].as<std::string>("");

        txn.commit();
        return detail;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in findById: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::vector<RecipeVideo> PgRecipeRepository::findVideos(int) {
    throw ServiceException("Not implemented", 501);
}

PagedRatings PgRecipeRepository::findRatings(int, int, int) {
    throw ServiceException("Not implemented", 501);
}

SubmitRecipeResponse PgRecipeRepository::create(int userId, const SubmitRecipeRequest& data) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        json ingredientsJson = json::array();
        for (const auto& ing : data.ingredients)
            ingredientsJson.push_back({{"name", ing.name}, {"quantity", ing.quantity}, {"unit", ing.unit}});

        json stepsJson = json::array();
        for (const auto& s : data.steps) {
            json step;
            step["order"] = s.order;
            step["description"] = s.description;
            if (s.duration.has_value())
                step["duration"] = s.duration.value();
            stepsJson.push_back(step);
        }

        json nutritionJson;
        if (data.nutrition.has_value()) {
            nutritionJson["calories"] = data.nutrition->calories;
            nutritionJson["protein"]  = data.nutrition->protein;
            nutritionJson["fat"]      = data.nutrition->fat;
            nutritionJson["carbs"]    = data.nutrition->carbs;
        } else {
            nutritionJson = json::object();
        }

        pqxx::result r = txn.exec_params(
            "INSERT INTO recipes (name, description, image_url,"
            " ingredients, steps, nutrition_info, tags,"
            " cooking_method, flavor, ingredient_type,"
            " author_id, status)"
            " VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, 'pending')"
            " RETURNING id",
            data.name,
            data.description,
            data.image_url,
            ingredientsJson.dump(),
            stepsJson.dump(),
            nutritionJson.dump(),
            data.tags,
            data.cooking_method.has_value() ? data.cooking_method.value() : "",
            data.flavor.has_value() ? data.flavor.value() : "",
            data.ingredient_type.has_value() ? data.ingredient_type.value() : "",
            userId);

        txn.commit();

        SubmitRecipeResponse resp;
        resp.id = r[0][0].as<int>();
        resp.status = "pending";
        return resp;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in create recipe: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

PagedMyRecipes PgRecipeRepository::findMySubmittedRecipes(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}

void PgRecipeRepository::update(int, int, const EditRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgRecipeRepository::toggleFavorite(int userId, int recipeId, std::optional<int> groupId, std::optional<bool> isPublic) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // Check if already favorited
        pqxx::result existing = txn.exec_params(
            "SELECT id FROM favorites WHERE user_id = $1 AND recipe_id = $2",
            userId, recipeId
        );

        if (existing.empty()) {
            // Insert new favorite
            int gid = groupId.has_value() ? groupId.value() : 0;
            bool pub = isPublic.has_value() ? isPublic.value() : true;

            if (gid > 0) {
                txn.exec_params(
                    "INSERT INTO favorites (user_id, recipe_id, group_id, is_public) VALUES ($1, $2, $3, $4)",
                    userId, recipeId, gid, pub
                );
            } else {
                txn.exec_params(
                    "INSERT INTO favorites (user_id, recipe_id, is_public) VALUES ($1, $2, $3)",
                    userId, recipeId, pub
                );
            }
        } else {
            // Already favorited → unfavorite (toggle off)
            txn.exec_params(
                "DELETE FROM favorites WHERE user_id = $1 AND recipe_id = $2",
                userId, recipeId
            );
        }
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in toggleFavorite: %s", e.what());
        throw ServiceException("操作失败");
    }
}

void PgRecipeRepository::rateRecipe(int, int, const RateRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgRecipeRepository::updateRating(int, int, int, const RateRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgRecipeRepository::deleteRating(int, int, int) {
    throw ServiceException("Not implemented", 501);
}

PagedUserRatings PgRecipeRepository::findMyRatings(int, int, int) {
    throw ServiceException("Not implemented", 501);
}

NutritionReport PgRecipeRepository::findNutrition(int) {
    throw ServiceException("Not implemented", 501);
}
