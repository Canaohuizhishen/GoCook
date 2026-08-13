#pragma once

#include <gocook/IMealPlanRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 膳食计划仓库的 PostgreSQL 实现，承载 IMealPlanRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 MealPlanServiceImpl 调用。
 */
class PgMealPlanRepository : public gocook::repository::IMealPlanRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgMealPlanRepository(ConnectionPool& db) : db_(db) {}

    // 创建膳食计划项，返回计划项 ID
    int createMealPlan(int userId,
                       const gocook::models::MealPlanRequest& planData) override;

    // 查询指定日期范围的膳食计划列表（分页）
    gocook::models::MealPlansResponse findMealPlans(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) override;

    // 查询膳食计划日历视图详情（分页，按天聚合）
    gocook::models::PagedCalendarDays findMealPlanDetail(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) override;

    // 更新膳食计划项
    void updateMealPlan(int userId, int planId,
                        const gocook::models::MealPlanRequest& updates) override;

    // 删除膳食计划项
    void deleteMealPlan(int userId, int planId) override;

    // 查询指定日期范围的营养摄入趋势
    gocook::models::NutritionTrendResponse findNutritionTrend(
        int userId, const std::string& startDate,
        const std::string& endDate) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
