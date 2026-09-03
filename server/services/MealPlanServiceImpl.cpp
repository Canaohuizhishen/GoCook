#include "MealPlanServiceImpl.h"

using namespace gocook::services;
using namespace gocook::models;

MealPlanServiceImpl::MealPlanServiceImpl(std::unique_ptr<gocook::repository::IMealPlanRepository> mealPlanRepo)
    : mealPlanRepo_(std::move(mealPlanRepo)) {}

int MealPlanServiceImpl::createMealPlan(int, const MealPlanRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

MealPlansResponse MealPlanServiceImpl::getMealPlans(int, const std::string&,
                                                     const std::string&, int, int) {
    throw ServiceException("功能暂未实现", 501);
}

PagedCalendarDays MealPlanServiceImpl::getMealPlanDetail(int, const std::string&,
                                                          const std::string&, int, int) {
    throw ServiceException("功能暂未实现", 501);
}

void MealPlanServiceImpl::updateMealPlan(int, int, const MealPlanRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void MealPlanServiceImpl::deleteMealPlan(int, int) {
    throw ServiceException("功能暂未实现", 501);
}

NutritionTrendResponse MealPlanServiceImpl::getNutritionTrend(int, const std::string&,
                                                               const std::string&) {
    throw ServiceException("功能暂未实现", 501);
}
