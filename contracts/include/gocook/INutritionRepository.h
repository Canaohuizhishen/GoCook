#pragma once

#include <gocook/DataModels.h>
#include <string>
#include <vector>
#include <optional>

namespace gocook::repository {

/**
 * @brief 食材营养条目（每 100g 含量）。
 *
 * 由 server/repositories/PgIngredientNutritionRepository 实现（PostgreSQL），
 * 供 RecipeServiceImpl 在投稿/编辑时按食材清单计算菜谱营养。
 */
struct IngredientNutrition {
    std::string name;               ///< 规范名（如"鸡蛋"）
    double default_portion_g = 0.0; ///< 单个计数的近似质量（克）；0 表示无法按个数换算
    double calories = 0.0;          ///< 热量（千卡 / 100g）
    double protein_g = 0.0;         ///< 蛋白质（克 / 100g）
    double fat_g = 0.0;             ///< 脂肪（克 / 100g）
    double carbs_g = 0.0;           ///< 碳水化合物（克 / 100g）
    double fiber_g = 0.0;           ///< 膳食纤维（克 / 100g）
    double sodium_mg = 0.0;         ///< 钠（毫克 / 100g）
    double vitamin_c_mg = 0.0;      ///< 维生素 C（毫克 / 100g）
};

/**
 * @brief 食材营养数据访问抽象接口。
 *
 * 只承担"按名称查营养表"一个职责；名称规范化、单位换算与累加等
 * 业务计算由 RecipeServiceImpl 完成。
 */
class INutritionRepository {
public:
    virtual ~INutritionRepository() = default;

    /**
     * @brief 按名称（含别名）批量查询食材营养。
     * @param names 规范化后的食材名称列表（原样传入，不做大小写/去空格处理）
     * @return 与 names 按序对齐的结果：匹配到返回营养条目（规范名），
     *         未匹配返回 nullopt。实现必须保持顺序（一次批量查询：VALUES 行号
     *         对齐 + LATERAL 去重，无 N+1，每个入参至多一行、规范名优先）。
     */
    virtual std::vector<std::optional<IngredientNutrition>> findByNames(
        const std::vector<std::string>& names) = 0;
};

} // namespace gocook::repository
