#pragma once

#include <gocook/DataModels.h>
#include <string>

namespace gocook::repository {

class IMealPlanRepository {
public:
    virtual ~IMealPlanRepository() = default;

    virtual int createMealPlan(int userId,
                               const models::MealPlanRequest& planData) = 0;

    virtual models::MealPlansResponse findMealPlans(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) = 0;

    virtual models::PagedCalendarDays findMealPlanDetail(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) = 0;

    virtual void updateMealPlan(int userId, int planId,
                                const models::MealPlanRequest& updates) = 0;

    virtual void deleteMealPlan(int userId, int planId) = 0;

    virtual models::NutritionTrendResponse findNutritionTrend(
        int userId, const std::string& startDate,
        const std::string& endDate) = 0;
};

} // namespace gocook::repository
