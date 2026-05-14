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
    Router(ConnectionPool& db,
           RecipeHandler& recipeHandler,
           UserHandler& userHandler,
           InventoryHandler& inventoryHandler,
           MealPlanHandler& mealPlanHandler,
           AnnouncementHandler& announcementHandler,
           AdminHandler& adminHandler);

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

    static std::vector<RateLimiter::Rule> createRateLimiterRules();

    void registerRateLimiter(httplib::Server& svr);
    void registerRootRoute(httplib::Server& svr);
    void registerAuthRoutes(httplib::Server& svr);
    void registerRecipeRoutes(httplib::Server& svr);
    void registerUserRoutes(httplib::Server& svr);
    void registerInventoryRoutes(httplib::Server& svr);
    void registerShoppingListRoutes(httplib::Server& svr);
    void registerMealPlanRoutes(httplib::Server& svr);
    void registerAnnouncementRoutes(httplib::Server& svr);
    void registerAdminRoutes(httplib::Server& svr);
    void registerPublicTestRoutes(httplib::Server& svr);
};
