#pragma once

#include <gocook/IServices.h>
#include <gocook/IMealPlanRepository.h>
#include <memory>

/**
 * @brief 膳食计划服务实现，承载 IMealPlanService 接口定义的全部业务逻辑。
 *
 * 依赖膳食计划仓库抽象完成数据访问，由 MealPlanHandler 调用。
 */
class MealPlanServiceImpl : public gocook::services::IMealPlanService {
public:
    // 构造函数，注入膳食计划仓库抽象
    explicit MealPlanServiceImpl(std::unique_ptr<gocook::repository::IMealPlanRepository> mealPlanRepo);

    // ---------- IMealPlanService 接口实现 ----------
    // 创建膳食计划项，返回计划项 ID
    int createMealPlan(int userId,
                       const gocook::models::MealPlanRequest& planData) override;
    // 获取指定日期范围的膳食计划列表（分页）
    gocook::models::MealPlansResponse getMealPlans(
        int userId,
        const std::string& startDate,
        const std::string& endDate,
        int page, int size) override;
    // 获取膳食计划日历视图详情（分页，按天聚合）
    gocook::models::PagedCalendarDays getMealPlanDetail(
        int userId,
        const std::string& startDate,
        const std::string& endDate,
        int page, int size) override;
    // 更新膳食计划项
    void updateMealPlan(int userId, int planId,
                        const gocook::models::MealPlanRequest& updates) override;
    // 删除膳食计划项
    void deleteMealPlan(int userId, int planId) override;
    // 获取指定日期范围的营养摄入趋势
    gocook::models::NutritionTrendResponse getNutritionTrend(
        int userId,
        const std::string& startDate,
        const std::string& endDate) override;

private:
    std::unique_ptr<gocook::repository::IMealPlanRepository> mealPlanRepo_; ///< 膳食计划仓库抽象
};
