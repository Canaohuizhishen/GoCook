#pragma once

#include <gocook/DataModels.h>
#include <string>

namespace gocook::repository {

/**
 * @brief 膳食计划数据访问抽象接口，定义膳食计划域的全部持久化操作。
 *
 * 由 server/repositories/PgMealPlanRepository 实现（PostgreSQL），
 * 供 MealPlanServiceImpl 依赖注入调用。
 */
class IMealPlanRepository {
public:
    virtual ~IMealPlanRepository() = default;

    /**
     * @brief 创建膳食计划项。
     * @param userId 用户 ID
     * @param planData 计划数据（菜谱/日期/餐次）
     * @return 计划项 ID
     */
    virtual int createMealPlan(int userId,
                               const models::MealPlanRequest& planData) = 0;

    /**
     * @brief 查询指定日期范围的膳食计划列表（分页）。
     * @param userId 用户 ID
     * @param startDate 开始日期（YYYY-MM-DD）
     * @param endDate 结束日期（YYYY-MM-DD）
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的膳食计划列表（含营养汇总）
     */
    virtual models::MealPlansResponse findMealPlans(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) = 0;

    /**
     * @brief 查询膳食计划日历视图详情（分页，按天聚合）。
     * @param userId 用户 ID
     * @param startDate 开始日期（YYYY-MM-DD）
     * @param endDate 结束日期（YYYY-MM-DD）
     * @param page 页码（从 1 开始）
     * @param size 每页天数
     * @return 分页的日历天数列表（含每日营养合计）
     */
    virtual models::PagedCalendarDays findMealPlanDetail(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) = 0;

    /**
     * @brief 更新膳食计划项。
     * @param userId 用户 ID
     * @param planId 计划项 ID
     * @param updates 待更新的字段
     */
    virtual void updateMealPlan(int userId, int planId,
                                const models::MealPlanRequest& updates) = 0;

    /**
     * @brief 删除膳食计划项。
     * @param userId 用户 ID
     * @param planId 计划项 ID
     */
    virtual void deleteMealPlan(int userId, int planId) = 0;

    /**
     * @brief 查询指定日期范围的营养摄入趋势。
     * @param userId 用户 ID
     * @param startDate 开始日期（YYYY-MM-DD）
     * @param endDate 结束日期（YYYY-MM-DD）
     * @return 趋势数据（每日实际摄入对比推荐目标）
     */
    virtual models::NutritionTrendResponse findNutritionTrend(
        int userId, const std::string& startDate,
        const std::string& endDate) = 0;
};

} // namespace gocook::repository
