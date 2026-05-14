#pragma once

#include <gocook/IServices.h>
#include <gocook/IMealPlanRepository.h>
#include <memory>

class MealPlanServiceImpl : public gocook::services::IMealPlanService {
public:
    explicit MealPlanServiceImpl(std::unique_ptr<gocook::repository::IMealPlanRepository> mealPlanRepo);

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
    std::unique_ptr<gocook::repository::IMealPlanRepository> mealPlanRepo_;
};