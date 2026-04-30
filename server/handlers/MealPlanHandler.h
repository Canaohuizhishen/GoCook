#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>          // 依赖抽象 IMealPlanService
#include <nlohmann/json.hpp>
#include "../auth_middleware.h"

class MealPlanHandler {
public:
    explicit MealPlanHandler(gocook::services::IMealPlanService& service,
                             AuthMiddleware& auth);

    // 创建膳食计划项（需认证）
    void createMealPlan(const httplib::Request& req, httplib::Response& res);
    // 获取膳食计划列表（需认证，分页）
    void getMealPlans(const httplib::Request& req, httplib::Response& res);
    // 获取膳食计划日历视图详情（需认证）
    void getMealPlanDetail(const httplib::Request& req, httplib::Response& res);
    // 更新膳食计划项（需认证）
    void updateMealPlan(const httplib::Request& req, httplib::Response& res);
    // 删除膳食计划项（需认证）
    void deleteMealPlan(const httplib::Request& req, httplib::Response& res);
    // 获取营养摄入趋势（需认证）
    void getNutritionTrend(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IMealPlanService& service_;   // 业务抽象
    AuthMiddleware& auth_;                         // 认证中间件
};