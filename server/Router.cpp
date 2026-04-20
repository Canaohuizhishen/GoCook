// Router.cpp
#include "Router.h"

Router::Router(DBConnection& db,
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
    adminHandler_(adminHandler) {}

void Router::setupRoutes(httplib::Server& svr) {
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
    // 关联视频
    svr.Get(R"(/api/recipes/(\d+)/videos)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeVideos(req, res);
    });
    // 评分与评论列表
    svr.Get(R"(/api/recipes/(\d+)/ratings)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeRatings(req, res);
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
    svr.Get("/api/users/me/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getPreferences(req, res);
    });
    svr.Put("/api/users/me/preferences", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updatePreferences(req, res);
    });
    svr.Put("/api/users/me/health-profile", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateHealthProfile(req, res);
    });
    svr.Get("/api/users/me/favorites", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getFavorites(req, res);
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
    // 获取购物清单
    svr.Get("/api/inventory/shopping-list", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getShoppingList(req, res);
    });
    // 更新购物清单项状态
    svr.Patch(R"(/api/inventory/shopping-list/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.updateShoppingListItem(req, res);
    });
    // 批量添加购物清单项
    svr.Post("/api/inventory/shopping-list/items/batch", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.batchAddShoppingItems(req, res);
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

    // ============================================================
    // 公开测试接口
    // ============================================================
    // 库存公开测试接口
    svr.Get("/api/inventory/public", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            pqxx::work txn(db_.getConn());
            pqxx::result userRes = txn.exec(
                "SELECT id FROM users WHERE username = " + txn.quote("testuser"));
            if (userRes.empty()) {
                res.status = 404;
                res.body = json{{"error", "Test user 'testuser' not found. Please run seed_test_data.sql"}}.dump();
                return;
            }
            int testUserId = userRes[0]["id"].as<int>();

            auto rows = txn.exec(
                "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
                "FROM inventory WHERE user_id = " + txn.quote(testUserId) + " ORDER BY added_at DESC");

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
            pqxx::work txn(db_.getConn());
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