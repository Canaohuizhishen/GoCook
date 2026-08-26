#pragma once

#include <gocook/INutritionRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 食材营养仓库的 PostgreSQL 实现，承载 INutritionRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 RecipeServiceImpl 在投稿/编辑时调用。
 */
class PgIngredientNutritionRepository : public gocook::repository::INutritionRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgIngredientNutritionRepository(ConnectionPool& db) : db_(db) {}

    // 按名称（含别名）批量查询食材营养，结果与入参按序对齐（未匹配为 nullopt）
    std::vector<std::optional<gocook::repository::IngredientNutrition>> findByNames(
        const std::vector<std::string>& names) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
