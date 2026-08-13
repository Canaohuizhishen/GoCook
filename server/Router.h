#pragma once

#include <httplib/httplib.h>
#include "common/ConnectionPool.h"
#include "handlers/RecipeHandler.h"
#include "handlers/UserHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MealPlanHandler.h"
#include "handlers/AnnouncementHandler.h"
#include "handlers/AdminHandler.h"
#include "middleware/RateLimiter.h"

/**
 * @brief HTTP 路由注册器，负责把各业务 Handler 挂载到 httplib::Server 上。
 *
 * 持有连接池与全部 Handler 的引用，按业务域分组注册路由，
 * 并负责全局限流规则的配置与挂载。
 */
class Router {
public:
    /**
     * @brief 构造函数，注入连接池与各业务 Handler。
     * @param db 数据库连接池
     * @param recipeHandler 菜谱处理器
     * @param userHandler 用户处理器
     * @param inventoryHandler 库存处理器
     * @param mealPlanHandler 膳食计划处理器
     * @param announcementHandler 公告处理器
     * @param adminHandler 管理员处理器
     */
    Router(ConnectionPool& db,
           RecipeHandler& recipeHandler,
           UserHandler& userHandler,
           InventoryHandler& inventoryHandler,
           MealPlanHandler& mealPlanHandler,
           AnnouncementHandler& announcementHandler,
           AdminHandler& adminHandler);

    // 注册全部路由到服务器实例
    void setupRoutes(httplib::Server& svr);

private:
    ConnectionPool& db_;                        ///< 数据库连接池
    RecipeHandler& recipeHandler_;              ///< 菜谱处理器
    UserHandler& userHandler_;                  ///< 用户处理器
    InventoryHandler& inventoryHandler_;        ///< 库存处理器
    MealPlanHandler& mealPlanHandler_;          ///< 膳食计划处理器
    AnnouncementHandler& announcementHandler_;  ///< 公告处理器
    AdminHandler& adminHandler_;                ///< 管理员处理器

    RateLimiter rateLimiter_;                   ///< 全局限流器

    // 构建全局限流规则（按路径前缀匹配）
    static std::vector<RateLimiter::Rule> createRateLimiterRules();

    // 注册全局限流中间件
    void registerRateLimiter(httplib::Server& svr);
    // 注册根路由（健康检查）
    void registerRootRoute(httplib::Server& svr);
    // 注册头像静态文件路由
    void registerAvatarFileRoutes(httplib::Server& svr);
    // 注册菜谱图片静态文件路由
    void registerRecipeFileRoutes(httplib::Server& svr);
    // 注册认证相关路由（注册/登录/密码重置）
    void registerAuthRoutes(httplib::Server& svr);
    // 注册菜谱相关路由
    void registerRecipeRoutes(httplib::Server& svr);
    // 注册用户相关路由
    void registerUserRoutes(httplib::Server& svr);
    // 注册库存相关路由
    void registerInventoryRoutes(httplib::Server& svr);
    // 注册购物清单相关路由
    void registerShoppingListRoutes(httplib::Server& svr);
    // 注册膳食计划相关路由
    void registerMealPlanRoutes(httplib::Server& svr);
    // 注册公告相关路由
    void registerAnnouncementRoutes(httplib::Server& svr);
    // 注册管理员相关路由
    void registerAdminRoutes(httplib::Server& svr);
    // 注册公开测试路由（调试用）
    void registerPublicTestRoutes(httplib::Server& svr);
};
