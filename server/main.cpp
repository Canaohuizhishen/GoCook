#include <httplib/httplib.h>
#include <iostream>
#include "DBConnection.h"
#include "services/RecipeServiceImpl.h"
#include "services/UserServiceImpl.h"
#include "services/InventoryServiceImpl.h"
#include "services/MealPlanServiceImpl.h"
#include "services/AnnouncementServiceImpl.h"
#include "services/AdminServiceImpl.h"
#include "handlers/RecipeHandler.h"
#include "handlers/UserHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MealPlanHandler.h"
#include "handlers/AnnouncementHandler.h"
#include "handlers/AdminHandler.h"
#include "auth_middleware.h"
#include "Router.h"

int main() {
    std::string connStr = "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";
    DBConnection db(connStr);
    if (!db.connect()) {
        std::cerr << "Cannot connect to database.\n";
        return 1;
    }

    // 创建认证中间件，后续通过构造函数注入给各 Handler
    AuthMiddleware authMiddleware("GoCook-Project-Secret-Key-Change-Me-In-Production");

    // 创建 Service 实现
    RecipeServiceImpl recipeService(db);
    UserServiceImpl userService(db);
    InventoryServiceImpl inventoryService(db);
    MealPlanServiceImpl mealPlanService(db);
    AnnouncementServiceImpl announcementService(db);
    AdminServiceImpl adminService(db);

    // 创建 Handler，注入服务抽象与认证中间件
    RecipeHandler recipeHandler(recipeService, authMiddleware);
    UserHandler userHandler(userService, authMiddleware);
    InventoryHandler inventoryHandler(inventoryService, authMiddleware);
    MealPlanHandler mealPlanHandler(mealPlanService, authMiddleware);
    AnnouncementHandler announcementHandler(announcementService);
    AdminHandler adminHandler(adminService, authMiddleware);

    // 构造 Router，注入所有 Handler
    Router router(db,
                  recipeHandler,
                  userHandler,
                  inventoryHandler,
                  mealPlanHandler,
                  announcementHandler,
                  adminHandler);

    httplib::Server svr;
    router.setupRoutes(svr);

    std::cout << "Server started on http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}