#include "MealPlanHandler.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MealPlanHandler::MealPlanHandler(gocook::services::IMealPlanService& service,
                                 AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void MealPlanHandler::createMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getMealPlans(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getMealPlanDetail(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::updateMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::deleteMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void MealPlanHandler::getNutritionTrend(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}