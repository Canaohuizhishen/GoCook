#include "PgMealPlanRepository.h"
#include <gocook/IServices.h>

using namespace gocook::repository;
using namespace gocook::services;
using namespace gocook::models;

int PgMealPlanRepository::createMealPlan(int, const MealPlanRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

MealPlansResponse PgMealPlanRepository::findMealPlans(int, const std::string&,
                                                       const std::string&, int, int) {
    throw ServiceException("功能暂未实现", 501);
}

PagedCalendarDays PgMealPlanRepository::findMealPlanDetail(int, const std::string&,
                                                            const std::string&, int, int) {
    throw ServiceException("功能暂未实现", 501);
}

void PgMealPlanRepository::updateMealPlan(int, int, const MealPlanRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgMealPlanRepository::deleteMealPlan(int, int) {
    throw ServiceException("功能暂未实现", 501);
}

NutritionTrendResponse PgMealPlanRepository::findNutritionTrend(int, const std::string&,
                                                                 const std::string&) {
    throw ServiceException("功能暂未实现", 501);
}
