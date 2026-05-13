#include "MealPlanHandler.h"
#include <nlohmann/json.hpp>
#include "../common/ErrorHelper.h"

using json = nlohmann::json;
using namespace gocook::models;

static json toJson(const Nutrition& n) {
    return {{"calories", n.calories}, {"protein", n.protein}, {"fat", n.fat}, {"carbs", n.carbs}};
}

static json toJson(const RecipeSummary& recipe) {
    json item;
    item["id"] = recipe.id;
    item["name"] = recipe.name;
    item["description"] = recipe.description;
    item["image_url"] = recipe.image_url;
    item["prep_time_minutes"] = recipe.prep_time_minutes;
    item["cook_time_minutes"] = recipe.cook_time_minutes;
    item["tags"] = recipe.tags;
    item["author_id"] = recipe.author_id;
    item["author_name"] = recipe.author_name;
    return item;
}

static json toJson(const MealPlanSummary& plan) {
    json obj;
    obj["plan_id"] = plan.plan_id;
    obj["date"] = plan.date;
    obj["meal_type"] = plan.meal_type;
    obj["recipe"] = toJson(plan.recipe);
    obj["nutrition"] = toJson(plan.nutrition);
    return obj;
}

static json toJson(const Pagination& pag) {
    return {{"page", pag.page}, {"size", pag.size}, {"total", pag.total}, {"total_pages", pag.total_pages}};
}

static json toJson(const MealPlansResponse& resp) {
    json obj;
    obj["data"] = json::array();
    for (const auto& p : resp.data)
        obj["data"].push_back(toJson(p));
    if (resp.nutrition_summary.has_value()) {
        obj["nutrition_summary"] = {
            {"total_calories", resp.nutrition_summary->total_calories},
            {"avg_protein", resp.nutrition_summary->avg_protein}
        };
    }
    obj["pagination"] = toJson(resp.pagination);
    return obj;
}

static json toJson(const DailyMealDetails& meals) {
    json m;
    if (meals.breakfast.has_value()) m["breakfast"] = toJson(meals.breakfast.value());
    if (meals.lunch.has_value()) m["lunch"] = toJson(meals.lunch.value());
    if (meals.dinner.has_value()) m["dinner"] = toJson(meals.dinner.value());
    if (meals.snack.has_value()) m["snack"] = toJson(meals.snack.value());
    return m;
}

static json toJson(const CalendarDay& day) {
    return {
        {"date", day.date},
        {"meals", toJson(day.meals)},
        {"daily_total", toJson(day.daily_total)}
    };
}

static json toJson(const PagedCalendarDays& paged) {
    json obj;
    obj["data"] = json::array();
    for (const auto& day : paged.data)
        obj["data"].push_back(toJson(day));
    obj["pagination"] = toJson(paged.pagination);
    return obj;
}

static json toJson(const NutritionComparisonItem& comp) {
    return {{"diff", comp.diff}, {"percentage", comp.percentage}};
}

static json toJson(const DailyNutritionComparison& comp) {
    return {
        {"calories", toJson(comp.calories)},
        {"protein", toJson(comp.protein)},
        {"fat", toJson(comp.fat)},
        {"carbs", toJson(comp.carbs)}
    };
}

static json toJson(const NutritionTrendItem& item) {
    return {
        {"date", item.date},
        {"actual", toJson(item.actual)},
        {"recommended", toJson(item.recommended)},
        {"comparison", toJson(item.comparison)}
    };
}

static json toJson(const NutritionTrendResponse& trend) {
    json arr = json::array();
    for (const auto& t : trend.trend)
        arr.push_back(toJson(t));
    return {
        {"trend", arr},
        {"daily_goals", toJson(trend.daily_goals)}
    };
}

MealPlanHandler::MealPlanHandler(gocook::services::IMealPlanService& service,
                                 AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void MealPlanHandler::createMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        json reqJson = json::parse(req.body);
        MealPlanRequest plan;
        plan.recipe_id = reqJson.at("recipe_id").get<int>();
        plan.date = reqJson.at("date").get<std::string>();
        plan.meal_type = reqJson.at("meal_type").get<std::string>();
        int planId = service_.createMealPlan(info.userId, plan);
        res.status = 201;
        res.body = json{{"plan_id", planId}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void MealPlanHandler::getMealPlans(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        std::string start = req.get_param_value("start_date");
        std::string end = req.get_param_value("end_date");
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        auto result = service_.getMealPlans(info.userId, start, end, page, size);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void MealPlanHandler::getMealPlanDetail(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        std::string start = req.get_param_value("start_date");
        std::string end = req.get_param_value("end_date");
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 7;
        auto result = service_.getMealPlanDetail(info.userId, start, end, page, size);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void MealPlanHandler::updateMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        int planId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        MealPlanRequest updates;
        if (reqJson.contains("recipe_id")) updates.recipe_id = reqJson["recipe_id"];
        if (reqJson.contains("date")) updates.date = reqJson["date"];
        if (reqJson.contains("meal_type")) updates.meal_type = reqJson["meal_type"];
        service_.updateMealPlan(info.userId, planId, updates);
        res.status = 200;
        res.body = json{{"message", "膳食计划已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void MealPlanHandler::deleteMealPlan(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        int planId = std::stoi(req.matches[1]);
        service_.deleteMealPlan(info.userId, planId);
        res.status = 200;
        res.body = json{{"message", "膳食计划已删除"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void MealPlanHandler::getNutritionTrend(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        std::string start = req.get_param_value("start_date");
        std::string end = req.get_param_value("end_date");
        auto trend = service_.getNutritionTrend(info.userId, start, end);
        res.status = 200;
        res.body = toJson(trend).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}