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

        pqxx::result countRes = txn.exec("SELECT COUNT(*) FROM recipes");
        int total = countRes[0][0].as<int>();

        std::string query = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url, r.tags, r.author_id,
                   u.username AS author_name
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
            ORDER BY r.id
            LIMIT $1 OFFSET $2
        )";
        auto rows = txn.exec_params(query, size, offset);

        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>();
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>();
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            // 解析 PostgreSQL TEXT[] 数组，格式例如：{中式,快手}
            auto tagsField = row["tags"];
            if (!tagsField.is_null()) {
                // 转换为字符串并去掉首尾大括号
                std::string rawTags = tagsField.c_str();
                if (rawTags.size() >= 2 && rawTags.front() == '{' && rawTags.back() == '}') {
                    rawTags = rawTags.substr(1, rawTags.size() - 2);
                }
                // 按逗号分割并去除元素两侧的引号和空格
                std::istringstream stream(rawTags);
                std::string token;
                while (std::getline(stream, token, ',')) {
                    // 去掉可能的首尾空格和引号
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