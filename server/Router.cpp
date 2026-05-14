#include "Router.h"

Router::Router(ConnectionPool& db,
               RecipeHandler& recipeHandler,
               UserHandler& userHandler,
               InventoryHandler& inventoryHandler,
               MealPlanHandler& mealPlanHandler,
               AnnouncementHandler& announcementHandler,
               AdminHandler& adminHandler)
    : db_(db),
    recipeHandler_(recipeHandler),
    userHandler_(userHandler),
    inventoryHandler_(inventoryHandler),
    mealPlanHandler_(mealPlanHandler),
    announcementHandler_(announcementHandler),
    adminHandler_(adminHandler),
    rateLimiter_(createRateLimiterRules())   // 通过工厂函数创建，业务规则外置
{
}

std::vector<RateLimiter::Rule> Router::createRateLimiterRules()
{
    using namespace std::chrono;
    return {
        // 认证接口：严格限制，每分钟最多 5 次，防止暴力破解
        {"/api/login",    seconds(60), 5},
        {"/api/register", seconds(60), 5},
        {"/api/password", seconds(60), 3},  // 密码重置为安全敏感路径，限额更低
        {"/api/admin",    seconds(60), 30},
        // 其他所有 API 接口：每分钟 60 次（适合普通用户正常使用）
        {"/api",          seconds(60), 60}
    };
}

void Router::setupRoutes(httplib::Server& svr) {
    // 注册全局频率限制中间件
    svr.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        std::string ip = req.remote_addr;
        if (ip.empty()) {
            ip = "127.0.0.1";   // 本地调试保护
        }
        if (!rateLimiter_.isAllowed(ip, req.path)) {
            res.status = 429;
            res.set_content(R"({"error":"Too many requests. Please try again later."})", "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // 根路径（公开）
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>GoCook Server</title>
</head>
<body>
    <h2>GoCook Server is running</h2>
    <p>以下为公开测试接口（无需认证）：</p>
    <ul>
        <li><a href="/api/recipes/public">GET /api/recipes/public</a> — 公开菜谱列表</li>
        <li><a href="/api/inventory/public">GET /api/inventory/public</a> — 公开库存列表（测试用户）</li>
        <li><a href="/api/users/public">GET /api/users/public</a> — 公开用户列表</li>
    </ul>
</html>
    )";
        res.set_content(html, "text/html");
    });

    // ============================================================
    // 用户认证（公开）
    // ============================================================
    svr.Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.registerUser(req, res);
    });
    svr.Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.loginUser(req, res);
    });

    // 忘记密码 / 重置密码（公开）
    svr.Post("/api/password/forgot", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.forgotPassword(req, res);
    });
    svr.Post("/api/password/reset", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.resetPassword(req, res);
    });

    // ============================================================
    // 菜谱相关（RecipeHandler）
    // ============================================================
    // 公开菜谱列表
    svr.Get("/api/recipes/public", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipesPublic(req, res);
    });
    // 搜索菜谱
    svr.Get("/api/recipes/search", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.searchRecipes(req, res);
    });
    // 智能推荐
    svr.Get("/api/recipes/recommend", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecommendedRecipes(req, res);
    });
    // 菜谱详情
    svr.Get(R"(/api/recipes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeDetail(req, res);
    });
    // 菜谱营养报告（v2.8 新增）
    svr.Get(R"(/api/recipes/(\d+)/nutrition)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeNutrition(req, res);
    });
    // 关联视频
    svr.Get(R"(/api/recipes/(\d+)/videos)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeVideos(req, res);
    });
    // 评分与评论列表
    svr.Get(R"(/api/recipes/(\d+)/ratings)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeRatings(req, res);
    });
    // 修改评论
    svr.Put(R"(/api/recipes/(\d+)/ratings/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.updateRating(req, res);
    });
    // 删除评论
    svr.Delete(R"(/api/recipes/(\d+)/ratings/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.deleteRating(req, res);
    });
    // 投稿新菜谱
    svr.Post("/api/recipes", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.submitRecipe(req, res);
    });
    // 我的投稿列表
    svr.Get("/api/recipes/my", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getMySubmittedRecipes(req, res);
    });
    // 编辑未审核菜谱
    svr.Put(R"(/api/recipes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.editRecipe(req, res);
    });
    // 收藏/取消收藏
    svr.Post(R"(/api/recipes/(\d+)/favorite)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.toggleFavorite(req, res);
    });
    // 评分与评论
    svr.Post(R"(/api/recipes/(\d+)/rate)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.rateRecipe(req, res);
    });

    // ============================================================
    // 用户相关（UserHandler，需认证的由 Handler 内部校验）
    // ============================================================
    svr.Get("/api/users/me", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getCurrentUser(req, res);
    });
    svr.Put("/api/users/me/profile", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateProfile(req, res);
    });
    // 头像上传
    svr.Post("/api/users/me/avatar", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.uploadAvatar(req, res);
    });
    // 修改密码
    svr.Put("/api/users/me/password", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.changePassword(req, res);
    });
    // 注销账户
    svr.Delete("/api/users/me", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.deleteAccount(req, res);
    });
    // 饮食偏好
    svr.Get("/api/users/me/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getPreferences(req, res);
    });
    svr.Put("/api/users/me/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updatePreferences(req, res);
    });
    // 健康指标
    svr.Put("/api/users/me/health-profile", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateHealthProfile(req, res);
    });
    // 收藏列表
    svr.Get("/api/users/me/favorites", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getFavorites(req, res);
    });
    // 收藏分组管理
    svr.Get("/api/users/me/favorites/groups", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getFavoriteGroups(req, res);
    });
    svr.Post("/api/users/me/favorites/groups", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.createFavoriteGroup(req, res);
    });
    svr.Put(R"(/api/users/me/favorites/groups/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateFavoriteGroup(req, res);
    });
    svr.Delete(R"(/api/users/me/favorites/groups/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.deleteFavoriteGroup(req, res);
    });
    // 更新单个收藏项属性（移动分组/可见性）
    svr.Patch(R"(/api/users/me/favorites/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateFavoriteItem(req, res);
    });
    // 批量删除收藏
    svr.Delete("/api/users/me/favorites/batch", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.batchDeleteFavorites(req, res);
    });
    // 通知中心
    svr.Get("/api/users/me/notifications", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getNotifications(req, res);
    });
    svr.Patch(R"(/api/users/me/notifications/(\d+)/read)", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.markNotificationRead(req, res);
    });
    svr.Put("/api/users/me/notifications/read-all", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.markAllNotificationsRead(req, res);
    });
    svr.Delete(R"(/api/users/me/notifications/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.deleteNotification(req, res);
    });
    // 我的评论列表
    svr.Get("/api/users/me/ratings", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getMyRatings(req, res);
    });

    // ============================================================
    // 库存管理（InventoryHandler）
    // ============================================================
    // 获取库存（需认证）
    svr.Get("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getInventory(req, res);
    });
    // 添加/更新库存项
    svr.Post("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.upsertInventory(req, res);
    });
    // 删除库存项
    svr.Delete(R"(/api/inventory/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.deleteInventory(req, res);
    });

    // ============================================================
    // 购物清单（多清单模型，需认证）
    // ============================================================
    // 获取用户的购物清单列表
    svr.Get("/api/inventory/shopping-lists", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getShoppingLists(req, res);
    });
    // 创建购物清单
    svr.Post("/api/inventory/shopping-lists", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.createShoppingList(req, res);
    });
    // 获取指定购物清单详情
    svr.Get(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getShoppingListDetail(req, res);
    });
    // 删除购物清单
    svr.Delete(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.deleteShoppingList(req, res);
    });
    // 更新购物清单项状态
    svr.Patch(R"(/api/inventory/shopping-lists/(\d+)/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.updateShoppingListItem(req, res);
    });
    // 批量添加购物清单项
    svr.Post(R"(/api/inventory/shopping-lists/(\d+)/items/batch)", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.batchAddShoppingItems(req, res);
    });
    // 导出购物清单
    svr.Get(R"(/api/inventory/shopping-lists/(\d+)/export)", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.exportShoppingList(req, res);
    });

    // ============================================================
    // 膳食计划（MealPlanHandler）
    // ============================================================
    svr.Post("/api/meal-plans", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.createMealPlan(req, res);
    });
    svr.Get("/api/meal-plans", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.getMealPlans(req, res);
    });
    svr.Get("/api/meal-plans/detail", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.getMealPlanDetail(req, res);
    });
    svr.Put(R"(/api/meal-plans/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.updateMealPlan(req, res);
    });
    svr.Delete(R"(/api/meal-plans/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.deleteMealPlan(req, res);
    });
    svr.Get("/api/meal-plans/nutrition-trend", [this](const httplib::Request& req, httplib::Response& res) {
        mealPlanHandler_.getNutritionTrend(req, res);
    });

    // ============================================================
    // 系统公告（AnnouncementHandler）
    // ============================================================
    svr.Get("/api/announcements", [this](const httplib::Request& req, httplib::Response& res) {
        announcementHandler_.getAnnouncements(req, res);
    });

    // ============================================================
    // 管理员功能（AdminHandler）
    // ============================================================
    // 用户管理
    svr.Get("/api/admin/users", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.getUsers(req, res);
    });
    svr.Post("/api/admin/users", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.createUser(req, res);
    });
    svr.Put(R"(/api/admin/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.updateUser(req, res);
    });
    svr.Patch(R"(/api/admin/users/(\d+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.setUserStatus(req, res);
    });
    svr.Delete(R"(/api/admin/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.deleteUser(req, res);
    });
    // 菜谱审核
    svr.Get("/api/admin/recipes/pending", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.getPendingRecipes(req, res);
    });
    svr.Post(R"(/api/admin/recipes/(\d+)/approve)", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.approveRecipe(req, res);
    });
    svr.Post(R"(/api/admin/recipes/(\d+)/reject)", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.rejectRecipe(req, res);
    });
    svr.Post("/api/admin/recipes/batch-review", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.batchReviewRecipes(req, res);
    });
    // 公告与通知
    svr.Post("/api/admin/announcements", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.publishAnnouncement(req, res);
    });
    svr.Post("/api/admin/notifications", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.sendNotification(req, res);
    });
    // 运营数据统计
    svr.Get("/api/admin/statistics", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.getStatistics(req, res);
    });
    // 管理员操作日志
    svr.Get("/api/admin/logs", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.getAdminLogs(req, res);
    });
    // 全局用户行为日志
    svr.Get("/api/admin/activity-logs", [this](const httplib::Request& req, httplib::Response& res) {
        adminHandler_.getActivityLogs(req, res);
    });

    // ============================================================
    // 公开测试接口
    // ============================================================
    // 库存公开测试接口
    svr.Get("/api/inventory/public", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto conn = db_.getConnection();
            pqxx::work txn(*conn);
            // 参数化查询避免注入
            pqxx::result userRes = txn.exec_params(
                "SELECT id FROM users WHERE username = $1", "testuser");
            if (userRes.empty()) {
                res.status = 404;
                res.body = json{{"error", "Test user 'testuser' not found. Please run seed_test_data.sql"}}.dump();
                return;
            }
            int testUserId = userRes[0]["id"].as<int>();

            auto rows = txn.exec_params(
                "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
                "FROM inventory WHERE user_id = $1 ORDER BY added_at DESC",
                testUserId);

            json result = json::array();
            for (const auto& row : rows) {
                json item;
                item["id"]              = row["id"].as<int>();
                item["ingredient_name"] = row["ingredient_name"].c_str();
                item["quantity"]        = row["quantity"].as<double>();
                item["unit"]            = row["unit"].c_str();
                if (!row["expiry_date"].is_null())
                    item["expiry_date"] = row["expiry_date"].c_str();
                item["added_at"]        = row["added_at"].c_str();
                result.push_back(item);
            }
            txn.commit();

            res.set_header("Content-Type", "application/json");
            res.status = 200;
            res.body = result.dump();
        } catch (const std::exception& e) {
            res.status = 500;
            res.body = json{{"error", e.what()}}.dump();
        }
    });

    // 用户公开测试接口
    svr.Get("/api/users/public", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto conn = db_.getConnection();
            pqxx::work txn(*conn);
            pqxx::result rows = txn.exec("SELECT id, username, created_at FROM users ORDER BY id");
            json users = json::array();
            for (const auto& row : rows) {
                json u;
                u["id"] = row["id"].as<int>();
                u["username"] = row["username"].c_str();
                u["created_at"] = row["created_at"].c_str();
                users.push_back(u);
            }
            txn.commit();
            res.set_header("Content-Type", "application/json");
            res.status = 200;
            res.body = users.dump();
        } catch (const std::exception& e) {
            res.status = 500;
            res.body = json{{"error", e.what()}}.dump();
        }
    });
}