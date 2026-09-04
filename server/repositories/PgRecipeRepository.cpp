#include "PgRecipeRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include "../common/DbExecutor.h"
#include "../common/UploadPaths.h"
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

//   本文件已按"生产级收敛形态"重构：所有常规方法用 executeDb 包裹（见 ../common/DbExecutor.h），
//   方法体只剩"差异部分"（SQL + 参数 + 行→结构体映射），异常分层/事务边界由辅助函数统一保证。
//   动态 WHERE 的骨架拼接（ParamBuilder）属于"差异部分"，留在本文件匿名命名空间里（见 4.3 节）。
//   三个特例保持手写（原因见方法内注释）：
//     · updateRecipeImage / updateStepImage —— 文件操作 + 数据库混编，失败文案自定义
//     · deleteRecipe —— 两段式：先读+提交 → 清理图片文件（文件系统操作不可回滚）→ 再执行 DELETE

namespace {

// 动态 WHERE 参数占位符发号器：拼的是"骨架"（结构由代码控制），值永远走参数化
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

// libpqxx 8：把字符串 vector 转成 pqxx::params
pqxx::params makeParams(const std::vector<std::string>& values) {
    pqxx::params p;
    for (const auto& v : values) p.append(v);
    return p;
}

// 把 SQL 返回的 tags JSON 数组文本解析成字符串列表
std::vector<std::string> parseTags(const std::string& jsonStr) {
    if (jsonStr.empty()) return {};
    auto arr = json::parse(jsonStr);
    std::vector<std::string> tags;
    for (const auto& t : arr)
        tags.push_back(t.get<std::string>());
    return tags;
}

// 安全读取 per_serving 数值字段：缺失 / null / 非数值一律按 0 处理。
// 写入路径恒产数字，但手改或历史数据可能混入 null/字符串，直接 .value() 会抛
// nlohmann::json::type_error 导致整页 500，这里统一兜底为"无数据"。
double safePerServingNum(const nlohmann::json& perServing, const char* key) {
    if (perServing.contains(key) && perServing[key].is_number())
        return perServing[key].get<double>();
    return 0.0;
}

// 判定 nutrition_info JSON 是否携带可用营养数据（findById 与 findNutrition 共用，
// 保证详情页与报告页对同一菜谱结论一致）：
// 入库形态唯一——含 per_serving 对象且七项（热量/蛋白/脂肪/碳水/纤维/钠/维C）任一 >0 即有数据；
// 空对象 {}、per_serving 为空对象或七项全 0、SQL NULL → 无数据（前端展示空态而非全 0）。
// 判定须覆盖纤维/钠/维C：纯调味料（如"盐 10 克"）宏量四项全 0 但钠有真实值，
// 只看宏量四项会把合法报告误判为"暂无营养报告"，故任一营养项 >0 即视为有数据。
// 客户端兜底（响应缺 has_data 时按 flat calories>0 判定）仅用于旧缓存/旧服务端兼容，
// 语义近似而非严格一致：新格式行两者结论一致（flat 与 per_serving 同源）。
bool nutritionHasData(const nlohmann::json& nutJson) {
    if (!nutJson.contains("per_serving") || !nutJson["per_serving"].is_object())
        return false;
    const auto& ps = nutJson["per_serving"];
    return safePerServingNum(ps, "calories") > 0.0
        || safePerServingNum(ps, "protein_g") > 0.0
        || safePerServingNum(ps, "fat_g") > 0.0
        || safePerServingNum(ps, "carbs_g") > 0.0
        || safePerServingNum(ps, "fiber_g") > 0.0
        || safePerServingNum(ps, "sodium_mg") > 0.0
        || safePerServingNum(ps, "vitamin_c_mg") > 0.0;
}

// 把筛选条件（cuisine/meal_type/difficulty/flavor/max_time/...）逐个拼进 WHERE 骨架，
// 占位符编号由 ParamBuilder 依次发放，值只进 values 最终走参数化执行（防注入分界线见 4.3 节）
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

bool PgRecipeRepository::existsByContent(const nlohmann::json& ingredients,
                                          const nlohmann::json& steps) {
    return executeDb(db_, [&](pqxx::work& txn) {
        std::string sql = R"(
            SELECT 1 FROM recipes
            WHERE ingredients = $1::jsonb
              AND COALESCE((
                SELECT jsonb_agg(elem - 'image_url' ORDER BY (elem->>'order')::int)
                FROM jsonb_array_elements(steps) AS elem
              ), '[]'::jsonb) = $2::jsonb
              AND status = 'approved'
            LIMIT 1
        )";

        auto rows = txn.exec(sql, pqxx::params{ingredients.dump(), steps.dump()});
        return !rows.empty();
    }, "数据库操作失败");
}

PagedRecipes PgRecipeRepository::findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) {
    return executeDb(db_, [&](pqxx::work& txn) {
        PagedRecipes result;
        int offset = (page > 0) ? (page - 1) * size : 0;

        std::string where = "WHERE r.status = 'approved'";
        ParamBuilder pb;

        applyRecipeFilters(filters, where, pb);

        std::string countSql = "SELECT COUNT(*) FROM recipes r " + where;
        int total;
        if (pb.values.empty()) {
            LOG_DEBUG("[SQL] findPublicRecipes count");
            total = txn.exec(countSql)[0][0].as<int>();
        } else {
            LOG_DEBUG("[SQL] findPublicRecipes count");
            total = txn.exec(countSql, makeParams(pb.values))
                        [0][0].as<int>();
        }

        // 排序字段白名单：只认 popular/rating/newest，其余回退默认（ORDER BY 不能拼用户输入）
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
                   -- 营养热量取整后再转 int：自动计算结果带 1 位小数（如 457.7），直接 as<int> 会转换失败
                   COALESCE(ROUND((r.nutrition_info->>'calories')::numeric), 0) AS calories,
                   r.author_id,
                   u.username AS author_name, r.cooking_method, r.flavor,
                   r.ingredient_type, r.view_count, r.avg_rating
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
        )" + where;

        LOG_DEBUG("[SQL] findPublicRecipes data");
        auto rows = txn.exec(dataSql, makeParams(pb.values));

        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>(0);
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>(0);
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            recipe.tags = parseTags(row["tags_json"].as<std::string>());
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
        result.pagination.total_pages = safeTotalPages(total, size);

        return result;
    }, "数据库操作失败");
}

PagedRecipes PgRecipeRepository::searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 初始化分页偏移与 WHERE 骨架
        PagedRecipes result;
        int offset = (page > 0) ? (page - 1) * size : 0;

        std::string where = "WHERE r.status = 'approved'";
        ParamBuilder pb;

        // 关键词模糊匹配：匹配菜名、描述、食材名称（ingredients 是 JSONB 数组）
        if (!keyword.empty()) {
            std::string safeKw = txn.esc(keyword);
            where += " AND (r.name ILIKE '%' || '" + safeKw + "' || '%'"  // 名字模糊匹配
                     " OR r.description ILIKE '%' || '" + safeKw + "' || '%'"  // 或描述模糊匹配
                     " OR EXISTS (SELECT 1 FROM jsonb_array_elements(r.ingredients) AS ing"  // 或食材名模糊匹配
                     "           WHERE ing->>'name' ILIKE '%' || '" + safeKw + "' || '%'))";
        }

        // 动态拼接筛选条件的SQL骨架
        applyRecipeFilters(filters, where, pb);

        // 总数查询——根据是否有参数，选择是否带参数执行
        std::string countSql = "SELECT COUNT(*) FROM recipes r " + where;
        int total;
        if (pb.values.empty()) {
            LOG_DEBUG("[SQL] searchRecipes count | keyword=%s", keyword.c_str());
            total = txn.exec(countSql)[0][0].as<int>();
        } else {
            LOG_DEBUG("[SQL] searchRecipes count | keyword=%s", keyword.c_str());
            total = txn.exec(countSql, makeParams(pb.values))
                        [0][0].as<int>();
        }

        // 排序白名单——防止 ORDER BY 注入
        std::string order = "ORDER BY r.created_at DESC";
        if (filters.contains("sort_by")) {
            std::string sort = filters["sort_by"].get<std::string>();
            if (sort == "popular") order = "ORDER BY r.view_count DESC";
            else if (sort == "rating") order = "ORDER BY r.avg_rating DESC";
            else if (sort == "newest") order = "ORDER BY r.created_at DESC";
        }

        // 用 ParamBuilder 生成 $n 占位符并添加 size 和 offset 值。
        where += " " + order + " LIMIT " + pb.next();
        pb.addInt(size);
        where += " OFFSET " + pb.next();
        pb.addInt(offset);

        // 构建最终 SQL（dataSql）并执行
        std::string dataSql = R"(
            SELECT r.id, r.name, r.description, r.prep_time_minutes,
                   r.cook_time_minutes, r.image_url,
                   array_to_json(r.tags) AS tags_json,
                   -- 营养热量取整后再转 int：自动计算结果带 1 位小数（如 457.7），直接 as<int> 会转换失败
                   COALESCE(ROUND((r.nutrition_info->>'calories')::numeric), 0) AS calories,
                   r.author_id,
                   u.username AS author_name, r.cooking_method, r.flavor,
                   r.ingredient_type, r.view_count, r.avg_rating
            FROM recipes r
            LEFT JOIN users u ON r.author_id = u.id
        )" + where;

        LOG_DEBUG("[SQL] searchRecipes data | keyword=%s", keyword.c_str());
        auto rows = txn.exec(dataSql, makeParams(pb.values));

        // 逐行读取查询结果（rows），构造 RecipeSummary 对象，添加到结果集（result）
        for (const auto& row : rows) {
            RecipeSummary recipe;
            recipe.id = row["id"].as<int>();
            recipe.name = row["name"].c_str();
            recipe.description = row["description"].c_str();
            recipe.prep_time_minutes = row["prep_time_minutes"].as<int>(0);
            recipe.cook_time_minutes = row["cook_time_minutes"].as<int>(0);
            if (!row["image_url"].is_null())
                recipe.image_url = row["image_url"].c_str();

            recipe.tags = parseTags(row["tags_json"].as<std::string>());
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

        // 分页信息填充
        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = safeTotalPages(total, size);

        return result;
    }, "数据库操作失败");
}

PagedRecommendedRecipes PgRecipeRepository::findRecommendedRecipes(int userId,
                                                                    int page,
                                                                    int size) {
    return executeDb(db_, [&](pqxx::work& txn) {
        PagedRecommendedRecipes result;
        int offset = (page > 0) ? (page - 1) * size : 0;

        // ── 总数查询 ──
        std::string countSql = R"(
            SELECT COUNT(DISTINCT r.id)
            FROM recipes r
            WHERE r.status = 'approved'
              AND jsonb_array_length(r.ingredients) > 0
        )";
        int total = txn.exec(countSql)[0][0].as<int>();
        LOG_DEBUG("[SQL] findRecommendedRecipes count: %d", total);

        // ── 数据查询：CTE 做库存匹配（推荐引擎核心）──
        std::string dataSql = R"(
            WITH inv_names AS (
                -- 匹配只看"有没有"：名字去重，防止同名多单位行（料酒 15毫升 + 100克）撑大计数
                SELECT DISTINCT LOWER(ingredient_name) AS name
                FROM inventory
                WHERE user_id = $1
            ),
            inv_display AS (
                -- 明细展示取最近录入的一行（数量/单位仅作参考展示，不作匹配依据）
                SELECT DISTINCT ON (LOWER(ingredient_name))
                       LOWER(ingredient_name) AS name,
                       quantity,
                       unit
                FROM inventory
                WHERE user_id = $1
                ORDER BY LOWER(ingredient_name), added_at DESC
            ),
            recipe_ings AS (
                SELECT
                    r.id AS recipe_id,
                    LOWER(ing->>'name') AS ing_name,
                    (ing->>'quantity')::numeric AS ing_qty,
                    ing->>'unit' AS ing_unit,
                    ing->>'name' AS ing_original_name
                FROM recipes r,
                     jsonb_array_elements(r.ingredients) AS ing
                WHERE r.status = 'approved'
                  AND jsonb_array_length(r.ingredients) > 0
            ),
            matched AS (
                SELECT
                    ri.recipe_id,
                    COUNT(*)::int AS total_count,
                    COUNT(CASE WHEN n.name IS NOT NULL THEN 1 END)::int AS match_count,
                    jsonb_agg(
                        CASE WHEN n.name IS NOT NULL THEN
                            jsonb_build_object(
                                'name', ri.ing_original_name,
                                'quantity', d.quantity,
                                'unit', d.unit
                            )
                        END
                    ) FILTER (WHERE n.name IS NOT NULL) AS available_json,
                    jsonb_agg(
                        CASE WHEN n.name IS NULL THEN
                            jsonb_build_object(
                                'name', ri.ing_original_name,
                                'quantity', ri.ing_qty,
                                'unit', ri.ing_unit
                            )
                        END
                    ) FILTER (WHERE n.name IS NULL) AS missing_json
                FROM recipe_ings ri
                LEFT JOIN inv_names n ON ri.ing_name = n.name
                LEFT JOIN inv_display d ON ri.ing_name = d.name
                GROUP BY ri.recipe_id
            )
            SELECT
                m.recipe_id,
                m.match_count,
                m.total_count,
                CASE WHEN m.total_count > 0
                     THEN ROUND((m.match_count::numeric / m.total_count), 2)
                     ELSE 0
                END AS match_score,
                m.available_json,
                m.missing_json,
                r.name,
                r.description,
                r.image_url,
                r.prep_time_minutes,
                r.cook_time_minutes,
                array_to_json(r.tags) AS tags_json,
                -- 营养热量取整后再转 int：自动计算结果带 1 位小数（如 457.7），直接 as<int> 会转换失败
                   COALESCE(ROUND((r.nutrition_info->>'calories')::numeric), 0) AS calories,
                r.author_id,
                u.username AS author_name,
                r.cooking_method,
                r.flavor,
                r.ingredient_type,
                r.view_count,
                r.avg_rating,
                COALESCE((r.nutrition_info->'per_serving'->>'protein_g')::numeric, 0) AS protein_g,
                COALESCE((r.nutrition_info->'per_serving'->>'fat_g')::numeric, 0) AS fat_g,
                COALESCE((r.nutrition_info->'per_serving'->>'carbs_g')::numeric, 0) AS carbs_g,
                COALESCE((r.nutrition_info->'per_serving'->>'sodium_mg')::numeric, 0) AS sodium_mg,
                r.submitted_at
            FROM matched m
            JOIN recipes r ON m.recipe_id = r.id
            LEFT JOIN users u ON r.author_id = u.id
            ORDER BY match_score DESC, r.avg_rating DESC, r.view_count DESC
            LIMIT $2 OFFSET $3
        )";

        LOG_DEBUG("[SQL] findRecommendedRecipes data | userId=%d", userId);
        auto rows = txn.exec(dataSql,
                                     pqxx::params{userId,
                                     size,
                                     offset});

        // 结果集映射
        for (const auto& row : rows) {
            RecommendedRecipe rec;

            // RecipeSummary 公共字段
            rec.id = row["recipe_id"].as<int>();
            rec.name = row["name"].is_null() ? "" : row["name"].c_str();
            rec.description = row["description"].is_null() ? "" : row["description"].c_str();
            rec.prep_time_minutes = row["prep_time_minutes"].as<int>(0);
            rec.cook_time_minutes = row["cook_time_minutes"].as<int>(0);
            if (!row["image_url"].is_null())
                rec.image_url = row["image_url"].c_str();
            rec.tags = parseTags(row["tags_json"].as<std::string>());
            rec.calories = row["calories"].as<int>(0);
            rec.author_id = row["author_id"].as<int>(0);
            rec.author_name = row["author_name"].is_null()
                ? "unknown" : row["author_name"].c_str();
            rec.cooking_method = row["cooking_method"].is_null()
                ? "" : row["cooking_method"].c_str();
            rec.flavor = row["flavor"].is_null()
                ? "" : row["flavor"].c_str();
            rec.ingredient_type = row["ingredient_type"].is_null()
                ? "" : row["ingredient_type"].c_str();
            rec.view_count = row["view_count"].as<int>(0);
            rec.avg_rating = row["avg_rating"].as<double>(0.0);

            // 原始匹配分（库存重合比例）
            rec.match_score = row["match_score"].as<double>(0.0);

            // 内部营养数据，供 Service 层打分用
            rec.protein_g = row["protein_g"].as<double>(0.0);
            rec.fat_g = row["fat_g"].as<double>(0.0);
            rec.carbs_g = row["carbs_g"].as<double>(0.0);
            rec.sodium_mg = row["sodium_mg"].as<double>(0.0);
            if (!row["submitted_at"].is_null())
                rec.submitted_at = row["submitted_at"].c_str();

            // MatchStatus：解析 available / missing 两个 JSON 数组
            if (!row["available_json"].is_null()) {
                auto jarr = json::parse(row["available_json"].c_str());
                for (const auto& j : jarr) {
                    MatchIngredient mi;
                    mi.name = j["name"].get<std::string>();
                    mi.quantity = j.value("quantity", 0.0);
                    mi.unit = j.value("unit", "");
                    rec.match_status.available_ingredients.push_back(std::move(mi));
                }
            }
            if (!row["missing_json"].is_null()) {
                auto jarr = json::parse(row["missing_json"].c_str());
                for (const auto& j : jarr) {
                    MissingIngredient mi;
                    mi.name = j["name"].get<std::string>();
                    mi.quantity = j.value("quantity", 0.0);
                    mi.unit = j.value("unit", "");
                    rec.match_status.missing_ingredients.push_back(std::move(mi));
                }
            }

            result.data.push_back(std::move(rec));
        }

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = safeTotalPages(total, size);
        result.health_filter_applied = false;  // 健康过滤由 Service 层设置

        return result;
    }, "数据库操作失败");
}

RecipeDetail PgRecipeRepository::findById(int recipeId, int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 判断是否需要查询收藏状态
        bool checkFav = (userId > 0);
        std::string favSelect = checkFav
            ? ", CASE WHEN f.id IS NOT NULL THEN true ELSE false END AS is_favorited"
            : "";
        std::string favJoin = checkFav
            ? " LEFT JOIN favorites f ON f.recipe_id = r.id AND f.user_id = $2"
            : "";
        // 拼接 SQL 主语句
        std::string sql = "SELECT r.id, r.name, r.description, r.image_url,"
            " r.cooking_method, r.flavor,"
            " r.prep_time_minutes, r.cook_time_minutes,"
            " r.view_count, r.avg_rating,"
            " r.ingredients, r.steps, r.nutrition_info,"
            " array_to_json(r.tags) AS tags_json,"
            " r.author_id, u.username AS author_name,"
            " r.created_at, r.updated_at"
            + favSelect
            + " FROM recipes r"
            + " LEFT JOIN users u ON r.author_id = u.id"
            + favJoin
            + " WHERE r.id = $1";

        // 执行查询
        LOG_DEBUG("[SQL] findById | recipeId=%d userId=%d", recipeId, userId);
        pqxx::result r;
        if (checkFav)
            r = txn.exec(sql, pqxx::params{recipeId, userId});
        else
            r = txn.exec(sql, pqxx::params{recipeId});

        if (r.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        const auto& row = r[0];

        // 填充结果
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

        // 解析 ingredients JSONB
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

        // 解析 steps JSONB
        if (!row["steps"].is_null()) {
            auto stepsArr = json::parse(row["steps"].c_str());
            for (const auto& s : stepsArr) {
                CookingStep step;
                step.order = s.value("order", 0);
                step.description = s.value("description", "");
                if (s.contains("duration"))
                    step.duration = s["duration"].get<int>();
                if (s.contains("image_url"))
                    step.image_url = s["image_url"].get<std::string>();
                detail.steps.push_back(step);
            }
        }

        // 解析 nutrition_info JSONB（has_data=false 表示无可用营养数据，前端展示空态而非全 0）。
        // 判定规则与 findNutrition 完全一致（共用 nutritionHasData，见匿名命名空间）：
        // 含 per_serving 对象且七项任一 >0 即有数据；空对象 {} / per_serving 空对象或全 0 → 无数据。
        if (!row["nutrition_info"].is_null()) {
            auto nutJson = json::parse(row["nutrition_info"].c_str());
            detail.nutrition.calories = nutJson.value("calories", 0.0);
            detail.nutrition.protein = nutJson.value("protein", 0.0);
            detail.nutrition.fat = nutJson.value("fat", 0.0);
            detail.nutrition.carbs = nutJson.value("carbs", 0.0);
            detail.nutrition.has_data = nutritionHasData(nutJson);
        } else {
            detail.nutrition.has_data = false;
        }

        // 填充 tags
        detail.tags = parseTags(row["tags_json"].as<std::string>());

        // 填充作者信息和时间戳
        detail.author_id = row["author_id"].as<int>(0);
        if (!row["author_name"].is_null())
            detail.author_name = row["author_name"].c_str();
        else
            detail.author_name = "unknown";

        detail.created_at = row["created_at"].as<std::string>("");
        detail.updated_at = row["updated_at"].as<std::string>("");

        return detail;
    }, "数据库操作失败");
}

std::vector<RecipeVideo> PgRecipeRepository::findVideos(int recipeId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] findVideos | recipeId=%d", recipeId);
        pqxx::result r = txn.exec(
            "SELECT id, title, platform, url, thumbnail_url, duration_seconds"
            " FROM recipe_videos"
            " WHERE recipe_id = $1"
            " ORDER BY id",
            pqxx::params{recipeId});

        std::vector<RecipeVideo> videos;
        for (const auto& row : r) {
            RecipeVideo v;
            v.id = row["id"].as<int>();
            v.title = row["title"].c_str();
            v.platform = row["platform"].c_str();
            v.url = row["url"].c_str();
            v.thumbnail_url = row["thumbnail_url"].as<std::string>("");
            v.duration_seconds = row["duration_seconds"].as<int>(0);
            videos.push_back(std::move(v));
        }
        return videos;
    }, "数据库操作失败");
}

PagedRatings PgRecipeRepository::findRatings(int recipeId, int page, int size) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 总数
        LOG_DEBUG("[SQL] findRatings count | recipeId=%d", recipeId);
        pqxx::result countResult = txn.exec(
            "SELECT COUNT(*) FROM ratings WHERE recipe_id = $1",
            pqxx::params{recipeId});
        int total = countResult[0][0].as<int>();

        // 分页数据
        int offset = (page - 1) * size;
        LOG_DEBUG("[SQL] findRatings data | recipeId=%d page=%d size=%d", recipeId, page, size);
        pqxx::result rows = txn.exec(
            "SELECT r.id, r.user_id, u.username, r.rating, r.comment, r.created_at"
            " FROM ratings r"
            " JOIN users u ON r.user_id = u.id"
            " WHERE r.recipe_id = $1"
            " ORDER BY r.created_at DESC"
            " LIMIT $2 OFFSET $3",
            pqxx::params{recipeId, size, offset});

        PagedRatings result;
        for (const auto& row : rows) {
            RecipeRating rating;
            rating.id = row["id"].as<int>();
            rating.user_id = row["user_id"].as<int>();
            rating.username = row["username"].c_str();
            rating.rating = row["rating"].as<int>();
            rating.comment = row["comment"].as<std::string>("");
            rating.created_at = row["created_at"].as<std::string>("");
            result.data.push_back(std::move(rating));
        }

        int totalPages = safeTotalPages(total, size);
        result.pagination = {page, size, total, totalPages};
        return result;
    }, "数据库操作失败");
}

SubmitRecipeResponse PgRecipeRepository::create(int userId, const SubmitRecipeRequest& data,
                                                const nlohmann::json& nutritionInfo) {
    return executeDb(db_, [&](pqxx::work& txn) {
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
            if (!s.image_url.empty())
                step["image_url"] = s.image_url;
            stepsJson.push_back(step);
        }

        // 营养 JSON 由 Service 层按食材清单计算好后传入（无数据时传空对象 {}）
        const json& nutritionJson = nutritionInfo;

        LOG_DEBUG("[SQL] INSERT recipes (create) | author_id=%d name=%s", userId, data.name.c_str());
        pqxx::result r = txn.exec(
            "INSERT INTO recipes (name, description, image_url,"
            " ingredients, steps, nutrition_info, tags,"
            " cooking_method, flavor, ingredient_type,"
            " author_id, status)"
            " VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, 'pending')"
            " RETURNING id",
            pqxx::params{data.name,
            data.description,
            data.image_url,
            ingredientsJson.dump(),
            stepsJson.dump(),
            nutritionJson.dump(),
            data.tags,
            data.cooking_method.has_value() ? data.cooking_method.value() : "",
            data.flavor.has_value() ? data.flavor.value() : "",
            data.ingredient_type.has_value() ? data.ingredient_type.value() : "",
            userId});

        SubmitRecipeResponse resp;
        resp.id = r[0][0].as<int>();
        resp.status = "pending";
        return resp;
    }, "数据库操作失败");
}

PagedMyRecipes PgRecipeRepository::findMySubmittedRecipes(int userId, int page, int size, const std::string& status) {
    return executeDb(db_, [&](pqxx::work& txn) {
        PagedMyRecipes result;
        int offset = (page > 0) ? (page - 1) * size : 0;

        std::string where = "WHERE author_id = $1";
        ParamBuilder pb;
        pb.addInt(userId);

        if (!status.empty()) {
            where += " AND status = " + pb.next();
            pb.add(status);
        }

        LOG_DEBUG("[SQL] findMySubmittedRecipes count | userId=%d", userId);
        std::string countSql = "SELECT COUNT(*) FROM recipes " + where;
        int total = txn.exec(countSql, makeParams(pb.values))
                        [0][0].as<int>();

        where += " ORDER BY updated_at DESC LIMIT " + pb.next();
        pb.addInt(size);
        where += " OFFSET " + pb.next();
        pb.addInt(offset);

        std::string dataSql = "SELECT id, name, status, reject_reason, submitted_at, updated_at"
                              " FROM recipes " + where;

        LOG_DEBUG("[SQL] findMySubmittedRecipes data | userId=%d", userId);
        auto rows = txn.exec(dataSql, makeParams(pb.values));

        for (const auto& row : rows) {
            MyRecipeStatus item;
            item.id = row["id"].as<int>();
            item.name = row["name"].c_str();
            item.status = row["status"].c_str();
            if (!row["reject_reason"].is_null())
                item.reject_reason = row["reject_reason"].c_str();
            item.submitted_at = row["submitted_at"].c_str();
            item.updated_at = row["updated_at"].c_str();
            result.data.push_back(std::move(item));
        }

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = safeTotalPages(total, size);

        return result;
    }, "数据库操作失败");
}

std::string PgRecipeRepository::update(int userId, int recipeId, const EditRecipeRequest& updates,
                                       const std::optional<nlohmann::json>& nutritionInfo) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 1. 验证菜谱存在、归属以及是否可编辑
        LOG_DEBUG("[SQL] SELECT author_id, status FROM recipes WHERE id = $1 (update verify) | $1=%d", recipeId);
        pqxx::result r = txn.exec(
            "SELECT author_id, status FROM recipes WHERE id = $1", pqxx::params{recipeId});

        if (r.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        int authorId = r[0]["author_id"].as<int>();
        std::string status = r[0]["status"].c_str();

        if (authorId != userId) {
            throw ServiceException("仅可编辑自己投稿的菜谱", 403);
        }

        // 2. 序列化 JSON 字段
        json ingredientsJson = json::array();
        for (const auto& ing : updates.ingredients)
            ingredientsJson.push_back({{"name", ing.name}, {"quantity", ing.quantity}, {"unit", ing.unit}});

        json stepsJson = json::array();
        for (const auto& s : updates.steps) {
            json step;
            step["order"] = s.order;
            step["description"] = s.description;
            if (s.duration.has_value())
                step["duration"] = s.duration.value();
            if (!s.image_url.empty())
                step["image_url"] = s.image_url;
            stepsJson.push_back(step);
        }

        // 营养 JSON 由 Service 层按新食材清单计算好后传入：
        //   has_value → 写入计算值（无数据时为 "{}"）；
        //   nullopt（营养计算不可用且无手填，如营养表查询异常）→ 传 SQL NULL，
        //   COALESCE 保留库中已有 nutrition_info，编辑不因计算不可用而清空营养。
        pqxx::params params;
        params.append(updates.name);
        params.append(updates.description);
        params.append(updates.image_url);
        params.append(ingredientsJson.dump());
        params.append(stepsJson.dump());
        if (nutritionInfo.has_value())
            params.append(nutritionInfo->dump());
        else
            params.append();   // SQL NULL
        params.append(updates.tags);
        params.append(updates.cooking_method.has_value() ? updates.cooking_method.value() : "");
        params.append(updates.flavor.has_value() ? updates.flavor.value() : "");
        params.append(updates.ingredient_type.has_value() ? updates.ingredient_type.value() : "");
        params.append(recipeId);

        // 3. 更新菜谱，编辑后始终回到 pending 状态等待重新审核
        LOG_DEBUG("[SQL] UPDATE recipes (update) | id=%d", recipeId);
        pqxx::result updateResult = txn.exec(
            "UPDATE recipes SET"
            " name = $1, description = $2, image_url = $3,"
            " ingredients = $4, steps = $5,"
            " nutrition_info = COALESCE($6, nutrition_info),"
            " tags = $7, cooking_method = $8, flavor = $9, ingredient_type = $10,"
            " status = 'pending', updated_at = NOW()"
            " WHERE id = $11"
            " RETURNING status",
            params);

        // 注意：必须在 lambda 内先拷贝成 std::string——updateResult 在 lambda 返回时就销毁，
        // 直接返回 c_str() 指针会悬空
        return std::string(updateResult[0]["status"].c_str());
    }, "数据库操作失败");
}

void PgRecipeRepository::toggleFavorite(int userId, int recipeId, std::optional<int> groupId, std::optional<bool> isPublic) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 查一下是否已收藏
        LOG_DEBUG("[SQL] toggleFavorite check | userId=%d recipeId=%d", userId, recipeId);
        pqxx::result existing = txn.exec(
            "SELECT id FROM favorites WHERE user_id = $1 AND recipe_id = $2",
            pqxx::params{userId, recipeId});

        if (existing.empty()) {
            // 未收藏 → 新增收藏
            int gid = groupId.has_value() ? groupId.value() : 0;
            bool pub = isPublic.has_value() ? isPublic.value() : true;

            if (gid > 0) {
                LOG_DEBUG("[SQL] toggleFavorite INSERT (with group) | userId=%d recipeId=%d", userId, recipeId);
                txn.exec(
                    "INSERT INTO favorites (user_id, recipe_id, group_id, is_public) VALUES ($1, $2, $3, $4)",
                    pqxx::params{userId, recipeId, gid, pub});
            } else {
                LOG_DEBUG("[SQL] toggleFavorite INSERT (no group) | userId=%d recipeId=%d", userId, recipeId);
                txn.exec(
                    "INSERT INTO favorites (user_id, recipe_id, is_public) VALUES ($1, $2, $3)",
                    pqxx::params{userId, recipeId, pub});
            }
        } else {
            // 已收藏 → 取消收藏（toggle 关）
            LOG_DEBUG("[SQL] toggleFavorite DELETE | userId=%d recipeId=%d", userId, recipeId);
            txn.exec(
                "DELETE FROM favorites WHERE user_id = $1 AND recipe_id = $2",
                pqxx::params{userId, recipeId});
        }
    }, "操作失败");
}

void PgRecipeRepository::rateRecipe(int userId, int recipeId,
                                    const RateRecipeRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 检查是否已评过分
        LOG_DEBUG("[SQL] rateRecipe check existing | userId=%d recipeId=%d", userId, recipeId);
        pqxx::result existing = txn.exec(
            "SELECT id FROM ratings WHERE user_id = $1 AND recipe_id = $2",
            pqxx::params{userId, recipeId});
        if (!existing.empty()) {
            throw ServiceException("您已评过分", 409);
        }

        // 插入评分记录
        LOG_DEBUG("[SQL] rateRecipe INSERT | userId=%d recipeId=%d rating=%d", userId, recipeId, req.rating);
        txn.exec(
            "INSERT INTO ratings (user_id, recipe_id, rating, comment)"
            " VALUES ($1, $2, $3, $4)",
            pqxx::params{userId, recipeId, req.rating, req.comment});

        // 更新菜谱平均分（同一事务：评分写进去但平均分没更新这种事不可能发生）
        LOG_DEBUG("[SQL] rateRecipe update avg_rating | recipeId=%d", recipeId);
        txn.exec(
            "UPDATE recipes SET avg_rating = ("
            "  SELECT COALESCE(ROUND(AVG(rating)::numeric, 1), 0.0)"
            "  FROM ratings WHERE recipe_id = $1"
            ") WHERE id = $1",
            pqxx::params{recipeId});
    }, "数据库操作失败");
}

void PgRecipeRepository::updateRating(int userId, int recipeId, int ratingId,
                                      const RateRecipeRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] updateRating | ratingId=%d userId=%d recipeId=%d", ratingId, userId, recipeId);
        pqxx::result r = txn.exec(
            "UPDATE ratings SET rating = $1, comment = $2, updated_at = NOW()"
            " WHERE id = $3 AND user_id = $4 AND recipe_id = $5"
            " RETURNING id",
            pqxx::params{req.rating, req.comment, ratingId, userId, recipeId});

        if (r.empty()) {
            throw ServiceException("无权限操作他人的评论", 403);
        }

        // 更新菜谱平均分
        LOG_DEBUG("[SQL] updateRating avg_rating | recipeId=%d", recipeId);
        txn.exec(
            "UPDATE recipes SET avg_rating = ("
            "  SELECT COALESCE(ROUND(AVG(rating)::numeric, 1), 0.0)"
            "  FROM ratings WHERE recipe_id = $1"
            ") WHERE id = $1",
            pqxx::params{recipeId});
    }, "数据库操作失败");
}

void PgRecipeRepository::deleteRating(int userId, int recipeId, int ratingId) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] deleteRating | ratingId=%d userId=%d recipeId=%d", ratingId, userId, recipeId);
        pqxx::result r = txn.exec(
            "DELETE FROM ratings WHERE id = $1 AND user_id = $2 AND recipe_id = $3"
            " RETURNING id",
            pqxx::params{ratingId, userId, recipeId});

        if (r.empty()) {
            throw ServiceException("无权限操作他人的评论", 403);
        }

        // 更新菜谱平均分
        LOG_DEBUG("[SQL] deleteRating avg_rating | recipeId=%d", recipeId);
        txn.exec(
            "UPDATE recipes SET avg_rating = ("
            "  SELECT COALESCE(ROUND(AVG(rating)::numeric, 1), 0.0)"
            "  FROM ratings WHERE recipe_id = $1"
            ") WHERE id = $1",
            pqxx::params{recipeId});
    }, "数据库操作失败");
}

std::optional<RecipeRating> PgRecipeRepository::findMyRating(int userId, int recipeId) {
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<RecipeRating> {
        LOG_DEBUG("[SQL] findMyRating | userId=%d recipeId=%d", userId, recipeId);
        pqxx::result r = txn.exec(
            "SELECT r.id, r.user_id, u.username, r.rating, r.comment, r.created_at"
            " FROM ratings r"
            " JOIN users u ON r.user_id = u.id"
            " WHERE r.recipe_id = $1 AND r.user_id = $2",
            pqxx::params{recipeId, userId});

        if (r.empty()) {
            return std::nullopt;
        }

        const auto& row = r[0];
        RecipeRating rating;
        rating.id = row["id"].as<int>();
        rating.user_id = row["user_id"].as<int>();
        rating.username = row["username"].c_str();
        rating.rating = row["rating"].as<int>();
        rating.comment = row["comment"].as<std::string>("");
        rating.created_at = row["created_at"].as<std::string>("");
        return rating;
    }, "数据库操作失败");
}

PagedUserRatings PgRecipeRepository::findMyRatings(int userId, int page, int size) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 总数
        LOG_DEBUG("[SQL] findMyRatings count | userId=%d", userId);
        pqxx::result countResult = txn.exec(
            "SELECT COUNT(*) FROM ratings WHERE user_id = $1",
            pqxx::params{userId});
        int total = countResult[0][0].as<int>();

        // 分页数据
        int offset = (page - 1) * size;
        LOG_DEBUG("[SQL] findMyRatings data | userId=%d page=%d size=%d", userId, page, size);
        pqxx::result rows = txn.exec(
            "SELECT r.id, r.recipe_id, rec.name AS recipe_name,"
            "       r.rating, r.comment, r.created_at, r.updated_at"
            " FROM ratings r"
            " JOIN recipes rec ON r.recipe_id = rec.id"
            " WHERE r.user_id = $1"
            " ORDER BY r.created_at DESC"
            " LIMIT $2 OFFSET $3",
            pqxx::params{userId, size, offset});

        PagedUserRatings result;
        for (const auto& row : rows) {
            UserRatingItem item;
            item.rating_id = row["id"].as<int>();
            item.recipe_id = row["recipe_id"].as<int>();
            item.recipe_name = row["recipe_name"].c_str();
            item.rating = row["rating"].as<int>();
            item.comment = row["comment"].as<std::string>("");
            item.created_at = row["created_at"].as<std::string>("");
            item.updated_at = row["updated_at"].as<std::string>("");
            result.data.push_back(std::move(item));
        }

        int totalPages = safeTotalPages(total, size);
        result.pagination = {page, size, total, totalPages};
        return result;
    }, "数据库操作失败");
}

NutritionReport PgRecipeRepository::findNutrition(int recipeId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 查询基础信息
        LOG_DEBUG("[SQL] findNutrition | recipeId=%d", recipeId);
        pqxx::result r = txn.exec(
            "SELECT r.id, r.name, r.nutrition_info"
            " FROM recipes r"
            " WHERE r.id = $1",
            pqxx::params{recipeId});

        if (r.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        const auto& row = r[0];

        NutritionReport report;
        report.recipe_id = row["id"].as<int>();
        report.recipe_name = row["name"].c_str();

        // 无营养数据（NULL）→ 返回 has_data=false，前端据此展示"暂无营养报告"空态；
        if (row["nutrition_info"].is_null()) {
            report.has_data = false;
            return report;
        }

        auto nutJson = json::parse(row["nutrition_info"].c_str());

        // 格式解析：入库形态唯一——含 per_serving 对象且七项任一 >0 即有数据
        // （判定见匿名命名空间 nutritionHasData，与 findById 共用保证两页结论一致）；
        // 空对象 {} / per_serving 空对象或全 0 → 无数据
        report.has_data = nutritionHasData(nutJson);
        if (report.has_data) {
            const auto& perServing = nutJson["per_serving"];
            report.per_serving.calories = safePerServingNum(perServing, "calories");
            report.per_serving.protein_g = safePerServingNum(perServing, "protein_g");
            report.per_serving.fat_g = safePerServingNum(perServing, "fat_g");
            report.per_serving.carbs_g = safePerServingNum(perServing, "carbs_g");
            report.per_serving.fiber_g = safePerServingNum(perServing, "fiber_g");
            report.per_serving.sodium_mg = safePerServingNum(perServing, "sodium_mg");
            report.per_serving.vitamin_c_mg = safePerServingNum(perServing, "vitamin_c_mg");
        }

        // 食材明细
        if (nutJson.contains("ingredients_breakdown")) {
            for (const auto& item : nutJson["ingredients_breakdown"]) {
                NutritionBreakdownItem bi;
                bi.name = item.value("name", "");
                bi.calories = item.value("calories", 0.0);
                bi.protein_g = item.value("protein_g", 0.0);
                bi.fat_g = item.value("fat_g", 0.0);
                bi.carbs_g = item.value("carbs_g", 0.0);
                report.ingredients_breakdown.push_back(std::move(bi));
            }
        }

        // 未计入营养的食材（恒为数组）
        if (nutJson.contains("excluded_ingredients") && nutJson["excluded_ingredients"].is_array()) {
            for (const auto& item : nutJson["excluded_ingredients"]) {
                ExcludedIngredient ei;
                ei.name = item.value("name", "");
                ei.reason = item.value("reason", "");
                report.excluded_ingredients.push_back(std::move(ei));
            }
        }

        // 健康提示
        report.health_notes = nutJson.value("health_notes", "");
        return report;
    }, "数据库操作失败");
}

// ⚠️ 特例：文件操作（拷贝/清理）+ 数据库更新混编，失败文案自定义（"菜谱图片保存失败"）。
// 保持手写——与 uploadAvatar 同理，特例显式化。
std::string PgRecipeRepository::updateRecipeImage(int recipeId, const std::string& filePath) {
    try {
        // 解析文件扩展名
        std::string ext = ".jpg";
        auto dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string lower;
            for (char c : filePath.substr(dotPos)) lower += std::tolower(static_cast<unsigned char>(c));
            if (lower == ".png")      ext = ".png";
            else if (lower == ".gif") ext = ".gif";
            else if (lower == ".bmp") ext = ".bmp";
            else if (lower == ".svg")  ext = ".svg";
        }

        // 生成唯一文件名：recipe_<id>_<时间戳><扩展名>
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      now.time_since_epoch()).count();
        std::string filename = "recipe_" + std::to_string(recipeId)
                             + "_" + std::to_string(ts) + ext;

        // 上传目录规则统一见 ../common/UploadPaths.h
        std::string uploadDir = UploadPaths::baseDir() + "/recipes/";
        std::filesystem::create_directories(uploadDir);
        std::string destPath = uploadDir + "/" + filename;

        // 把上传的临时文件复制到永久位置
        std::filesystem::copy(filePath, destPath,
                              std::filesystem::copy_options::overwrite_existing);

        std::string imageUrl = "/uploads/recipes/" + filename;

        // 更新数据库里的 image_url
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        txn.exec("UPDATE recipes SET image_url = $1 WHERE id = $2",
                        pqxx::params{imageUrl, recipeId});
        txn.commit();

        // 清理临时文件
        std::filesystem::remove(filePath);
        return imageUrl;

    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("更新菜谱主图时数据库出错：%s", e.what());
        throw ServiceException("菜谱图片保存失败");
    }
}

// ⚠️ 特例：文件操作 + 数据库更新混编，且要先读 JSONB steps 改完再写回，失败文案自定义
// （"步骤图片保存失败"）。保持手写。
std::string PgRecipeRepository::updateStepImage(int recipeId, int stepIndex, const std::string& filePath) {
    try {
        std::string ext = ".jpg";
        auto dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string lower;
            for (char c : filePath.substr(dotPos)) lower += std::tolower(static_cast<unsigned char>(c));
            if (lower == ".png")      ext = ".png";
            else if (lower == ".gif") ext = ".gif";
            else if (lower == ".bmp") ext = ".bmp";
            else if (lower == ".svg")  ext = ".svg";
        }

        // 生成唯一文件名：recipe_<id>_step_<索引>_<时间戳><扩展名>
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      now.time_since_epoch()).count();
        std::string filename = "recipe_" + std::to_string(recipeId)
                             + "_step_" + std::to_string(stepIndex)
                             + "_" + std::to_string(ts) + ext;

        // 上传目录规则统一见 ../common/UploadPaths.h
        std::string uploadDir = UploadPaths::baseDir() + "/recipes/";
        std::filesystem::create_directories(uploadDir);
        std::string destPath = uploadDir + "/" + filename;

        // 把上传的临时文件复制到永久位置
        std::filesystem::copy(filePath, destPath,
                              std::filesystem::copy_options::overwrite_existing);

        std::string imageUrl = "/uploads/recipes/" + filename;

        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // 读 JSONB steps，校验步骤索引，把 image_url 写进对应步骤再整体写回
        pqxx::result rows = txn.exec(
            "SELECT steps FROM recipes WHERE id = $1", pqxx::params{recipeId});
        if (rows.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        json steps = json::parse(rows[0]["steps"].as<std::string>("[]"));
        if (stepIndex < 0 || stepIndex >= (int)steps.size()) {
            throw ServiceException("步骤索引超出范围", 400);
        }

        steps[stepIndex]["image_url"] = imageUrl;

        txn.exec("UPDATE recipes SET steps = $1::jsonb WHERE id = $2",
                        pqxx::params{steps.dump(), recipeId});
        txn.commit();

        // 清理临时文件
        std::filesystem::remove(filePath);
        return imageUrl;

    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("更新步骤图时数据库出错：%s", e.what());
        throw ServiceException("步骤图片保存失败");
    }
}

// ⚠️ 特例：两段式删除（笔记 4.5 反例同款）——① 先在一个事务里读菜谱信息（校验权限 +
// 收集图片路径）并提交；② 清理图片文件（文件系统操作不能回滚）；③ 再开第二个事务执行
// DELETE。整个过程横跨两个连接和文件系统，无法用 executeDb 包裹，保持手写。
void PgRecipeRepository::deleteRecipe(int userId, int recipeId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // 读当前菜谱信息（检查权限 + 获取图片路径用于清理）
        pqxx::result rows = txn.exec(
            "SELECT author_id, status, image_url, steps FROM recipes WHERE id = $1",
            pqxx::params{recipeId});
        if (rows.empty()) {
            throw ServiceException("菜谱不存在", 404);
        }

        int authorId = rows[0]["author_id"].as<int>();
        if (authorId != userId) {
            throw ServiceException("无权删除他人的菜谱", 403);
        }

        std::string status = rows[0]["status"].as<std::string>("");
        if (status == "approved") {
            throw ServiceException("已通过审核的菜谱不能删除", 403);
        }

        // 收集所有图片路径用于清理
        std::vector<std::string> filesToRemove;

        try {
            std::string imageUrl = rows[0]["image_url"].as<std::string>("");
            if (!imageUrl.empty()) {
                // 存储 URL → 磁盘真实路径（统一规则见 ../common/UploadPaths.h）
                if (auto p = UploadPaths::urlToPath(imageUrl, "recipes"); !p.empty())
                    filesToRemove.push_back(p);
            }
        } catch (const std::exception& e) {
            // 路径换算异常（urlToPath 内 filesystem::absolute 在工作目录失效时会抛
            // filesystem_error）跳过主图清理，不阻塞删除主流程（与步骤图处理一致）
            LOG_WARN("解析菜谱主图 URL 失败，跳过清理: %s", e.what());
        }

        // 步骤图
        std::string stepsStr = rows[0]["steps"].as<std::string>("[]");
        if (!stepsStr.empty() && stepsStr != "[]") {
            try {
                json steps = json::parse(stepsStr);
                for (const auto& step : steps) {
                    if (step.contains("image_url") && !step["image_url"].is_null()) {
                        std::string imgUrl = step["image_url"].get<std::string>();
                        if (!imgUrl.empty()) {
                            if (auto p = UploadPaths::urlToPath(imgUrl, "recipes"); !p.empty())
                                filesToRemove.push_back(p);
                        }
                    }
                }
            } catch (const std::exception& e) {
                // steps 数据损坏（历史遗留）或路径换算异常（urlToPath 内 filesystem::absolute
                // 在工作目录失效时会抛 filesystem_error）均跳过步骤图清理，不阻塞删除主流程
                LOG_WARN("解析菜谱步骤图 URL 失败，跳过清理: %s", e.what());
            }
        }

        // 先提交（文件删除在提交之后——文件系统操作不能回滚）
        txn.commit();

        // 清理图片文件（失败不影响数据库删除）
        for (const auto& f : filesToRemove) {
            std::error_code ec;
            std::filesystem::remove(f, ec);
            if (ec) {
                LOG_WARN("删除图片文件失败: %s - %s", f.c_str(), ec.message().c_str());
            }
        }

        // 执行删除
        auto conn2 = db_.getConnection();
        pqxx::work txn2(*conn2);
        txn2.exec("DELETE FROM recipes WHERE id = $1 AND author_id = $2",
                         pqxx::params{recipeId, userId});
        txn2.commit();

    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("删除菜谱时数据库出错：%s", e.what());
        throw ServiceException("删除菜谱失败");
    }
}
