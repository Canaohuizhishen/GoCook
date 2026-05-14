#pragma once

#include <gocook/IServices.h>
#include "../ConnectionPool.h"

/**
 * @brief 膳食计划服务实现
 *
 * 当前所有方法均为骨架，抛出 ServiceException("Not implemented")。
 * 构造函数接收 ConnectionPool& 引用并保存为私有成员。
 */
class MealPlanServiceImpl : public gocook::services::IMealPlanService {
public:
    explicit MealPlanServiceImpl(ConnectionPool& db);

    // ---------- IMealPlanService 接口实现 ----------
    int createMealPlan(int userId,
                       const gocook::models::MealPlanRequest& planData) override;
    gocook::models::MealPlansResponse getMealPlans(
        int userId,
        const std::string& startDate,
        const std::string& endDate,
        int page, int size) override;
    gocook::models::PagedCalendarDays getMealPlanDetail(
        int userId,
        const std::string& startDate,
        const std::string& endDate,
        int page, int size) override;
    void updateMealPlan(int userId, int planId,
                        const gocook::models::MealPlanRequest& updates) override;
    void deleteMealPlan(int userId, int planId) override;
    gocook::models::NutritionTrendResponse getNutritionTrend(
        int userId,
        const std::string& startDate,
        const std::string& endDate) override;

private:
    ConnectionPool& db_;
};