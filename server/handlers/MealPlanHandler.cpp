#include "MealPlanHandler.h"

MealPlanHandler::MealPlanHandler(gocook::services::IMealPlanService& service)
    : service_(service) {}

void MealPlanHandler::createMealPlan(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getMealPlans(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getMealPlanDetail(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::updateMealPlan(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::deleteMealPlan(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getNutritionTrend(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}