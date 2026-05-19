#pragma once

#include <gocook/IMealPlanRepository.h>
#include "../common/ConnectionPool.h"

class PgMealPlanRepository : public gocook::repository::IMealPlanRepository {
public:
    explicit PgMealPlanRepository(ConnectionPool& db) : db_(db) {}

    int createMealPlan(int userId,
                       const gocook::models::MealPlanRequest& planData) override;

    gocook::models::MealPlansResponse findMealPlans(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) override;

    gocook::models::PagedCalendarDays findMealPlanDetail(
        int userId, const std::string& startDate,
        const std::string& endDate, int page, int size) override;

    void updateMealPlan(int userId, int planId,
                        const gocook::models::MealPlanRequest& updates) override;

    void deleteMealPlan(int userId, int planId) override;

    gocook::models::NutritionTrendResponse findNutritionTrend(
        int userId, const std::string& startDate,
        const std::string& endDate) override;

private:
    ConnectionPool& db_;
};
