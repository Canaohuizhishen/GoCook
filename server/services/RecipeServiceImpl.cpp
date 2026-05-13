#include "RecipeServiceImpl.h"
#include <pqxx/pqxx>

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::services;

namespace {

// 辅助类：自动管理 pqxx $N 参数占位符编号
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

    int count() const { return values.size(); }
};

// 辅助函数：从 tags_json (JSON 数组) 解析 tag 列表
std::vector<std::string> parseTags(const pqxx::field& field) {
    if (field.is_null()) return {};
    auto arr = json::parse(field.c_str());
    std::vector<std::string> tags;
    for (const auto& t : arr)
        tags.push_back(t.get<std::string>());
    return tags;
}

} // anonymous namespace

PagedRecipes RecipeServiceImpl::getPublicRecipes(int page, int size,
                                                 const nlohmann::json& filters) {
    PagedRecipes result;
    try {
        pqxx::work txn(db_.getConn());

        int offset = (page > 0) ? (page - 1) * size : 0;

        // 构建动态 WHERE 子句
        std::string where = "WHERE 1=1";
        ParamBuilder pb;

        auto addTagCond = [&](const std::string& value) {
            where += " AND " + pb.next() + " = ANY(r.tags)";
            pb.add(value);
        };

        if (filters.contains("cuisine"))         addTagCond(filters["cuisine"].get<std::string>());
        if (filters.contains("meal_type"))        addTagCond(filters["meal_type"].get<std::string>());
        if (filters.contains("difficulty"))       addTagCond(filters["difficulty"].get<std::string>());
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
        if (filters.contains("tags")) {
            for (const auto& tag : filters["tags"])
                addTagCond(tag.get<std::string>());
        }

        // 计算总条数（无参时用 exec 避免空 dynamic_params 崩溃）
        std::string countSql = "SELECT COUNT(*) FROM recipes r " + where;
        int total;
        if (pb.values.empty()) {
            total = txn.exec(countSql)[0][0].as<int>();
        } else {
            total = txn.exec_params(countSql, pqxx::prepare::make_dynamic_params(pb.values))
                        [0][0].as<int>();
        }

        // 构建动态 ORDER BY 子句
        std::string order = "ORDER BY r.created_at DESC";
        if (filters.contains("sort_by")) {
            std::string sort = filters["sort_by"].get<std::string>();
            if (sort == "popular") order = "ORDER BY r.view_count DESC";
            else if (sort == "rating") order = "ORDER BY r.avg_rating DESC";
            else if (sort == "newest") order = "ORDER BY r.created_at DESC";
        }

        // LIMIT / OFFSET 参数
        where += " " + order + " LIMIT " + pb.next();
        pb.addInt(size);
        where += " OFFSET " + pb.next();
        pb.addInt(offset);

        // 查询数据
        std::string dataSql = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url,
                   array_to_json(r.tags) AS tags_json,
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
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
    return result;
}