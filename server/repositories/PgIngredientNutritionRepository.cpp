#include "PgIngredientNutritionRepository.h"
#include <pqxx/pqxx>
#include "../common/Logger.h"
#include "../common/DbExecutor.h"

using namespace gocook::repository;

//   本文件按"生产级收敛形态"编写：方法用 executeDb 包裹（见 ../common/DbExecutor.h），
//   方法体只剩"差异部分"（SQL + 参数 + 行→结构体映射），异常分层/事务边界由辅助函数统一保证。

std::vector<std::optional<IngredientNutrition>> PgIngredientNutritionRepository::findByNames(
    const std::vector<std::string>& names)
{
    return executeDb(db_, [&](pqxx::work& txn) -> std::vector<std::optional<IngredientNutrition>> {
        if (names.empty()) return {};

        LOG_DEBUG("[SQL] findByNames | names=%zu", names.size());

        // VALUES 行按入参顺序编号（占位符 $1..$n 由循环发放，值全走参数化——防注入分界线同 4.3 节）；
        // LEFT JOIN LATERAL 用"规范名或别名"匹配，且每个入参至多返回一行：
        //   1) 防止一个输入名同时命中某行规范名与另一行别名时产生重复 ord 行、后续结果按序错位；
        //   2) 同一名字既命中规范名又命中别名时优先规范名（ORDER BY 规范名优先）。
        // 未命中的行各字段为 NULL，由 as<T>(默认值) 兜底后整行判空。
        std::string sql = "SELECT n.name, n.default_portion_g, n.calories, n.protein_g, n.fat_g,"
                          "       n.carbs_g, n.fiber_g, n.sodium_mg, n.vitamin_c_mg"
                          " FROM (VALUES ";
        pqxx::params params;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += "($" + std::to_string(i + 1) + "::text, " + std::to_string(i + 1) + ")";
            params.append(names[i]);
        }
        sql += ") AS q(name, ord)"
               " LEFT JOIN LATERAL ("
               "   SELECT name, default_portion_g, calories, protein_g, fat_g,"
               "          carbs_g, fiber_g, sodium_mg, vitamin_c_mg"
               "   FROM ingredient_nutrition n"
               "   WHERE n.name = q.name OR q.name = ANY(n.aliases)"
               "   ORDER BY (n.name = q.name) DESC, n.id"
               "   LIMIT 1"
               " ) n ON true"
               " ORDER BY q.ord";

        pqxx::result r = txn.exec(sql, params);

        std::vector<std::optional<IngredientNutrition>> result;
        result.reserve(r.size());
        for (const auto& row : r) {
            if (row["name"].is_null()) {
                result.push_back(std::nullopt);
                continue;
            }
            IngredientNutrition n;
            n.name = row["name"].c_str();
            n.default_portion_g = row["default_portion_g"].as<double>(0.0);
            n.calories   = row["calories"].as<double>(0.0);
            n.protein_g  = row["protein_g"].as<double>(0.0);
            n.fat_g      = row["fat_g"].as<double>(0.0);
            n.carbs_g    = row["carbs_g"].as<double>(0.0);
            n.fiber_g    = row["fiber_g"].as<double>(0.0);
            n.sodium_mg  = row["sodium_mg"].as<double>(0.0);
            n.vitamin_c_mg = row["vitamin_c_mg"].as<double>(0.0);
            result.push_back(std::move(n));
        }
        return result;
    }, "数据库操作失败");
}
