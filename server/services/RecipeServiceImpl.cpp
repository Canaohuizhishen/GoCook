#include "RecipeServiceImpl.h"
#include <pqxx/pqxx>
#include <sstream>

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::services;

PagedRecipes RecipeServiceImpl::getPublicRecipes(int page, int size,
                                                 const nlohmann::json& filters) {
    PagedRecipes result;
    try {
        pqxx::work txn(db_.getConn());

        int offset = (page > 0) ? (page - 1) * size : 0;

        // 构建动态 WHERE 子句
        std::string whereClause = "WHERE 1=1";
        std::vector<std::string> params;
        int paramCount = 0;

        if (filters.contains("cuisine")) {
            paramCount++;
            whereClause += " AND $"+std::to_string(paramCount)+" = ANY(r.tags)";
            params.push_back(filters["cuisine"].get<std::string>());
        }
        if (filters.contains("meal_type")) {
            paramCount++;
            whereClause += " AND $"+std::to_string(paramCount)+" = ANY(r.tags)";
            params.push_back(filters["meal_type"].get<std::string>());
        }
        if (filters.contains("flavor")) {
            paramCount++;
            whereClause += " AND r.flavor = $"+std::to_string(paramCount);
            params.push_back(filters["flavor"].get<std::string>());
        }
        if (filters.contains("cooking_method")) {
            paramCount++;
            whereClause += " AND r.cooking_method = $"+std::to_string(paramCount);
            params.push_back(filters["cooking_method"].get<std::string>());
        }
        if (filters.contains("ingredient_type")) {
            paramCount++;
            whereClause += " AND r.ingredient_type = $"+std::to_string(paramCount);
            params.push_back(filters["ingredient_type"].get<std::string>());
        }
        if (filters.contains("difficulty")) {
            paramCount++;
            whereClause += " AND $"+std::to_string(paramCount)+" = ANY(r.tags)";
            params.push_back(filters["difficulty"].get<std::string>());
        }
        if (filters.contains("max_time")) {
            paramCount++;
            whereClause += " AND (r.prep_time_minutes + r.cook_time_minutes) <= $"+std::to_string(paramCount);
            params.push_back(std::to_string(filters["max_time"].get<int>()));
        }
        if (filters.contains("min_calories")) {
            paramCount++;
            whereClause += " AND (r.nutrition_info->>'calories')::numeric >= $"+std::to_string(paramCount);
            params.push_back(std::to_string(filters["min_calories"].get<int>()));
        }
        if (filters.contains("max_calories")) {
            paramCount++;
            whereClause += " AND (r.nutrition_info->>'calories')::numeric <= $"+std::to_string(paramCount);
            params.push_back(std::to_string(filters["max_calories"].get<int>()));
        }
        if (filters.contains("tags")) {
            for (const auto& tag : filters["tags"]) {
                paramCount++;
                whereClause += " AND $"+std::to_string(paramCount)+" = ANY(r.tags)";
                params.push_back(tag.get<std::string>());
            }
        }

        // 计算总条数
        std::string countQueryStr = "SELECT COUNT(*) FROM recipes r " + whereClause;
        pqxx::result countRes;
        if (params.empty()) {
            countRes = txn.exec(countQueryStr);
        } else {
            countRes = txn.exec_params(countQueryStr, pqxx::prepare::make_dynamic_params(params));
        }
        int total = countRes[0][0].as<int>();

        // 构建动态 ORDER BY 子句
        std::string orderClause = "ORDER BY r.created_at DESC"; // 默认
        if (filters.contains("sort_by")) {
            std::string sort = filters["sort_by"].get<std::string>();
            if (sort == "popular") orderClause = "ORDER BY r.view_count DESC";
            else if (sort == "rating") orderClause = "ORDER BY r.avg_rating DESC";
            else if (sort == "newest") orderClause = "ORDER BY r.created_at DESC";
        }

        // 查询数据
        std::string dataQueryStr = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url, r.tags, r.author_id,
                   u.username AS author_name, r.cooking_method, r.flavor,
                   r.ingredient_type, r.view_count, r.avg_rating
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
        )" + whereClause + " " + orderClause + " LIMIT $" + std::to_string(paramCount+1) + " OFFSET $" + std::to_string(paramCount+2);

        params.push_back(std::to_string(size));
        params.push_back(std::to_string(offset));

        auto rows = txn.exec_params(dataQueryStr, pqxx::prepare::make_dynamic_params(params));

        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>();
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>();
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            auto tagsField = row["tags"];
            if (!tagsField.is_null()) {
                std::string rawTags = tagsField.c_str();
                if (rawTags.size() >= 2 && rawTags.front() == '{' && rawTags.back() == '}') {
                    rawTags = rawTags.substr(1, rawTags.size() - 2);
                }
                std::istringstream stream(rawTags);
                std::string token;
                while (std::getline(stream, token, ',')) {
                    while (!token.empty() && token.front() == '"') token.erase(0, 1);
                    while (!token.empty() && token.back() == '"') token.pop_back();
                    if (!token.empty()) {
                        recipe.tags.push_back(token);
                    }
                }
            }

            recipe.author_id = row["author_id"].as<int>(0);
            if (!row["author_name"].is_null())
                recipe.author_name = row["author_name"].c_str();
            else
                recipe.author_name = "unknown";

            // 新增字段
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
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
    return result;
}