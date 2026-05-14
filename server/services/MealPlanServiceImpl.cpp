#include "MealPlanServiceImpl.h"
#include <stdexcept>

using namespace gocook::services;
using namespace gocook::models;

MealPlanServiceImpl::MealPlanServiceImpl(ConnectionPool& db) : db_(db) {}

int MealPlanServiceImpl::createMealPlan(int userId,
                                        const MealPlanRequest& planData)
{
    throw ServiceException("Not implemented", 501);
}

MealPlansResponse MealPlanServiceImpl::getMealPlans(
    int userId,
    const std::string& startDate,
    const std::string& endDate,
    int page, int size)
{
    throw ServiceException("Not implemented", 501);
}

PagedCalendarDays MealPlanServiceImpl::getMealPlanDetail(
    int userId,
    const std::string& startDate,
    const std::string& endDate,
    int page, int size)
{
    throw ServiceException("Not implemented", 501);
}

void MealPlanServiceImpl::updateMealPlan(int userId, int planId,
                                         const MealPlanRequest& updates)
{
    throw ServiceException("Not implemented", 501);
}

void MealPlanServiceImpl::deleteMealPlan(int userId, int planId)
{
    throw ServiceException("Not implemented", 501);
}

NutritionTrendResponse MealPlanServiceImpl::getNutritionTrend(
    int userId,
    const std::string& startDate,
    const std::string& endDate)
{
    throw ServiceException("Not implemented", 501);
}