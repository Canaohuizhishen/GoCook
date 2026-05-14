#pragma once

#include <httplib/httplib.h>
#include "ConnectionPool.h"
#include "handlers/RecipeHandler.h"
#include "handlers/UserHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MealPlanHandler.h"
#include "handlers/AnnouncementHandler.h"
#include "handlers/AdminHandler.h"
#include "RateLimiter.h"

class Router {
public:
    // 构造函数接收所有 Handler 的引用
    Router(ConnectionPool& db,
           RecipeHandler& recipeHandler,
           UserHandler& userHandler,
           InventoryHandler& inventoryHandler,
           MealPlanHandler& mealPlanHandler,
           AnnouncementHandler& announcementHandler,
           AdminHandler& adminHandler);

    // 注册所有路由到 server 对象
    void setupRoutes(httplib::Server& svr);

private:
    ConnectionPool& db_;
    RecipeHandler& recipeHandler_;
    UserHandler& userHandler_;
    InventoryHandler& inventoryHandler_;
    MealPlanHandler& mealPlanHandler_;
    AnnouncementHandler& announcementHandler_;
    AdminHandler& adminHandler_;

    RateLimiter rateLimiter_;

    /**
     * @brief 创建限流规则集合的工厂函数
     *
     * 所有限流规则集中定义于此，方便统一管理与调整。
     * @return 规则列表
     */
    static std::vector<RateLimiter::Rule> createRateLimiterRules();
};