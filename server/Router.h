#pragma once

#include <httplib/httplib.h>
#include "DBConnection.h"
#include "handlers/RecipeHandler.h"
#include "handlers/UserHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MealPlanHandler.h"
#include "handlers/AnnouncementHandler.h"
#include "handlers/AdminHandler.h"

class Router {
public:
    // 构造函数接收所有 Handler 的引用
    Router(DBConnection& db,
           RecipeHandler& recipeHandler,
           UserHandler& userHandler,
           InventoryHandler& inventoryHandler,
           MealPlanHandler& mealPlanHandler,
           AnnouncementHandler& announcementHandler,
           AdminHandler& adminHandler);

    // 注册所有路由到 server 对象
    void setupRoutes(httplib::Server& svr);

private:
    DBConnection& db_;
    RecipeHandler& recipeHandler_;
    UserHandler& userHandler_;
    InventoryHandler& inventoryHandler_;
    MealPlanHandler& mealPlanHandler_;
    AnnouncementHandler& announcementHandler_;
    AdminHandler& adminHandler_;
};