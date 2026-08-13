#include "Router.h"
#include "common/Logger.h"
#include "common/ErrorHelper.h"
#include <filesystem>
#include <fstream>

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
    rateLimiter_(createRateLimiterRules())
{
}

std::vector<RateLimiter::Rule> Router::createRateLimiterRules()
{
    using namespace std::chrono;
    return {
        {"/api/login",    seconds(60), 5},
        {"/api/register", seconds(60), 5},
        {"/api/password", seconds(60), 3},
        {"/api/admin",    seconds(60), 30},
        {"/api",          seconds(60), 60}
    };
}

void Router::setupRoutes(httplib::Server& svr) {
    registerRateLimiter(svr);
    registerRootRoute(svr);
    registerAuthRoutes(svr);
    registerRecipeRoutes(svr);
    registerUserRoutes(svr);
    registerInventoryRoutes(svr);
    registerShoppingListRoutes(svr);
    registerMealPlanRoutes(svr);
    registerAnnouncementRoutes(svr);
    registerAdminRoutes(svr);
    registerPublicTestRoutes(svr);

    // 注册头像文件服务路由（替代 set_mount_point，更可靠且可控）
    registerAvatarFileRoutes(svr);
    // 注册菜谱图片文件服务路由
    registerRecipeFileRoutes(svr);
}

// ============================================================
// 全局频率限制中间件
// ============================================================

void Router::registerRateLimiter(httplib::Server& svr) {
    /*
     *  客户端 IP 来源：默认按 socket 对端（remote_addr）。
     *  反向代理部署时可设 GOCOOK_TRUST_PROXY_HEADERS=1 改用 X-Real-IP
     *  X-Forwarded-For 首个 IP——否则 docker-proxy/nginx 后所有用户共享同一计数。
     *  警告：该头由客户端可控，仅在可信代理之后启用。
     */

    /*
     * 此处使用“立即执行 Lambda (IIFE)”将环境变量读取与日志输出限定在
     * 静态局部变量初始化阶段，确保只在程序启动时执行一次：
     * 避免每次调用 registerRateLimiter 时重复读取 getenv 及刷写警告日志。
     */
    static const bool trustProxyHeaders = []() {
        const char* v = std::getenv("GOCOOK_TRUST_PROXY_HEADERS");
        bool on = v && std::string(v) == "1";
        if (on) {
            LOG_WARN("GOCOOK_TRUST_PROXY_HEADERS=1: 限流改用代理头(X-Real-IP/X-Forwarded-For)，"
                     "请确保服务端只暴露在可信反向代理之后，否则限流可被伪造头绕过");
        }
        return on;
    }();
    auto clientIp = [](const httplib::Request& req) -> std::string {
        if (trustProxyHeaders) {
            if (req.has_header("X-Real-IP"))
                return req.get_header_value("X-Real-IP");
            if (req.has_header("X-Forwarded-For")) {
                const std::string& xff = req.get_header_value("X-Forwarded-For");
                auto comma = xff.find(',');
                std::string first = comma == std::string::npos ? xff : xff.substr(0, comma);
                // 去掉可能的空白
                while (!first.empty() && (first.front() == ' ' || first.front() == '\t'))
                    first.erase(first.begin());
                while (!first.empty() && (first.back() == ' ' || first.back() == '\t'))
                    first.pop_back();
                if (!first.empty())
                    return first;
            }
        }
        return req.remote_addr.empty() ? "127.0.0.1" : req.remote_addr;
    };

    svr.set_pre_routing_handler([this, clientIp](const httplib::Request& req, httplib::Response& res) {
        const std::string ip = clientIp(req);
        LOG_DEBUG("%s %s from %s", req.method.c_str(), req.path.c_str(), ip.c_str());
        if (!rateLimiter_.isAllowed(ip, req.path)) {
            setErrorResponse(res, 429, "请求过于频繁，请稍后重试");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
}

// ============================================================
// 根路径（公开）
// ============================================================

void Router::registerRootRoute(httplib::Server& svr) {
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>GoCook API 接口一览</title>
</head>
<body>
<div class="container">
    <h1>GoCook API</h1>
    <h2>部分公开接口（用于测试）</h2>
    <div class="endpoint">GET</span><a href="/api/recipes/public">/api/recipes/public</a>公开菜谱列表</span></div>
    <div class="endpoint">GET</span><a href="/api/inventory/public">/api/inventory/public</a>公开库存（测试用户）</span></div>
    <div class="endpoint">GET</span><a href="/api/users/public">/api/users/public</a>公开用户列表</span></div>
    <div class="endpoint">GET</span><a href="/api/recipes/search?keyword=番茄">/api/recipes/search?keyword=</a>搜索菜谱（搜索关键词：番茄）</span></div>
    <div class="endpoint">GET</span><a href="/api/recipes/1">/api/recipes/:id</a>菜谱详情（1：番茄炒蛋）</span></div>
    <div class="endpoint">GET</span><a href="/api/recipes/1/nutrition">/api/recipes/:id/nutrition</a>菜谱营养报告（1：番茄炒蛋）</span></div>
    <div class="endpoint">GET</span><a href="/api/recipes/1/videos">/api/recipes/:id/videos</a>菜谱关联视频（1：番茄炒蛋）</span></div>
    <div class="endpoint">GET</span><a href="/api/recipes/1/ratings">/api/recipes/:id/ratings</a>菜谱评分与评论（1：番茄炒蛋）</span><span class="meta"></span></div>
    <div class="endpoint">GET</span><a href="/api/announcements">/api/announcements</a>系统公告</span><span class="meta"></span></div>
</div>
</html>
    )";
        res.set_content(html, "text/html");
    });
}

// ============================================================
// 头像等静态文件服务（替代 set_mount_point）
// ============================================================

void Router::registerAvatarFileRoutes(httplib::Server& svr) {
    // 上传目录：可从 GOCOOK_UPLOADS_DIR 环境变量覆盖，默认 "server/uploads"
    const char* envDir = std::getenv("GOCOOK_UPLOADS_DIR");
    std::string baseDir = envDir ? envDir : "server/uploads";
    std::string avatarDir = std::filesystem::absolute(baseDir + "/avatars/").string();
    try {
        std::filesystem::create_directories(avatarDir);
    } catch (const std::exception& e) {
        LOG_WARN("无法创建头像目录 %s: %s", avatarDir.c_str(), e.what());
    }
    LOG_INFO("Avatar directory: %s", avatarDir.c_str());

    // 服务头像文件：GET /uploads/avatars/<filename>
    svr.Get(R"(/uploads/avatars/(.+))", [avatarDir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string filename = req.matches[1];
            // 第一层防御：拦截基本路径穿越尝试（.. 和 /）
            if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
                res.status = 400;
                res.set_content("Bad request", "text/plain");
                return;
            }
            // 第二层防御：使用 weakly_canonical 验证最终路径仍在允许目录内
            std::string filePath = avatarDir + "/" + filename;
            std::filesystem::path normPath = std::filesystem::weakly_canonical(
                std::filesystem::path(filePath));
            std::filesystem::path allowedDir = std::filesystem::weakly_canonical(
                std::filesystem::path(avatarDir));
            auto normStr = normPath.string();
            auto allowStr = allowedDir.string();
            if (normStr.rfind(allowStr, 0) != 0) {
                LOG_WARN("Avatar path traversal blocked: %s → %s",
                         filename.c_str(), normStr.c_str());
                res.status = 400;
                res.set_content("Bad request", "text/plain");
                return;
            }
            if (!std::filesystem::exists(filePath)) {
                res.status = 404;
                res.set_content("Not found", "text/plain");
                return;
            }
            std::ifstream ifs(filePath, std::ios::binary);
            if (!ifs) {
                res.status = 500;
                res.set_content("Internal error", "text/plain");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            // 根据扩展名设置 MIME 类型
            auto dot = filename.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? filename.substr(dot) : "";
            std::string mime = "image/jpeg";
            if (ext == ".png")       mime = "image/png";
            else if (ext == ".gif")  mime = "image/gif";
            else if (ext == ".bmp")  mime = "image/bmp";
            else if (ext == ".webp") mime = "image/webp";
            else if (ext == ".svg")  mime = "image/svg+xml";

            res.set_content(content, mime);
        } catch (const std::exception& e) {
            LOG_ERROR("Error serving avatar file: %s", e.what());
            res.status = 500;
            res.set_content("Internal error", "text/plain");
        }
    });
}

void Router::registerRecipeFileRoutes(httplib::Server& svr) {
    const char* envDir = std::getenv("GOCOOK_UPLOADS_DIR");
    std::string baseDir = envDir ? envDir : "server/uploads";
    std::string recipeDir = std::filesystem::absolute(baseDir + "/recipes/").string();
    try {
        std::filesystem::create_directories(recipeDir);
    } catch (const std::exception& e) {
        LOG_WARN("无法创建菜谱图片目录 %s: %s", recipeDir.c_str(), e.what());
    }
    LOG_INFO("Recipe image directory: %s", recipeDir.c_str());

    svr.Get(R"(/uploads/recipes/(.+))", [recipeDir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string filename = req.matches[1];
            if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
                res.status = 400;
                res.set_content("Bad request", "text/plain");
                return;
            }
            std::string filePath = recipeDir + "/" + filename;
            std::filesystem::path normPath = std::filesystem::weakly_canonical(
                std::filesystem::path(filePath));
            std::filesystem::path allowedDir = std::filesystem::weakly_canonical(
                std::filesystem::path(recipeDir));
            auto normStr = normPath.string();
            auto allowStr = allowedDir.string();
            if (normStr.rfind(allowStr, 0) != 0) {
                res.status = 400;
                res.set_content("Bad request", "text/plain");
                return;
            }

            std::ifstream ifs(normStr, std::ios::binary);
            if (!ifs) {
                res.status = 404;
                res.set_content("Not found", "text/plain");
                return;
            }
            std::string content((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());

            auto dotPos = filename.find_last_of('.');
            std::string ext;
            if (dotPos != std::string::npos) {
                for (char c : filename.substr(dotPos)) ext += std::tolower(static_cast<unsigned char>(c));
            }
            std::string mime = "image/jpeg";
            if (ext == ".png")       mime = "image/png";
            else if (ext == ".gif")  mime = "image/gif";
            else if (ext == ".bmp")  mime = "image/bmp";
            else if (ext == ".webp") mime = "image/webp";
            else if (ext == ".svg")  mime = "image/svg+xml";

            res.set_content(content, mime);
        } catch (const std::exception& e) {
            LOG_ERROR("Error serving recipe image file: %s", e.what());
            res.status = 500;
            res.set_content("Internal error", "text/plain");
        }
    });
}

// ============================================================
// 用户认证（公开）
// ============================================================

void Router::registerAuthRoutes(httplib::Server& svr) {
    svr.Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.registerUser(req, res);
    });
    svr.Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.loginUser(req, res);
    });
    svr.Post("/api/password/forgot", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.forgotPassword(req, res);
    });
    svr.Post("/api/password/reset", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.resetPassword(req, res);
    });
}

// ============================================================
// 菜谱相关
// ============================================================

void Router::registerRecipeRoutes(httplib::Server& svr) {
    svr.Get("/api/recipes/public", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipesPublic(req, res);
    });
    svr.Get("/api/recipes/search", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.searchRecipes(req, res);
    });
    svr.Get("/api/recipes/recommend", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecommendedRecipes(req, res);
    });
    svr.Get(R"(/api/recipes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeDetail(req, res);
    });
    svr.Get(R"(/api/recipes/(\d+)/nutrition)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeNutrition(req, res);
    });
    svr.Get(R"(/api/recipes/(\d+)/videos)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeVideos(req, res);
    });
    svr.Get(R"(/api/recipes/(\d+)/ratings)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipeRatings(req, res);
    });
    svr.Get(R"(/api/recipes/(\d+)/ratings/mine)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getMyRecipeRating(req, res);
    });
    svr.Put(R"(/api/recipes/(\d+)/ratings/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.updateRating(req, res);
    });
    svr.Delete(R"(/api/recipes/(\d+)/ratings/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.deleteRating(req, res);
    });
    svr.Post("/api/recipes", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.submitRecipe(req, res);
    });
    svr.Get("/api/recipes/my", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getMySubmittedRecipes(req, res);
    });
    svr.Put(R"(/api/recipes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.editRecipe(req, res);
    });
    svr.Post(R"(/api/recipes/(\d+)/favorite)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.toggleFavorite(req, res);
    });
    svr.Post(R"(/api/recipes/(\d+)/rate)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.rateRecipe(req, res);
    });
    svr.Post(R"(/api/recipes/(\d+)/image)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.uploadRecipeImage(req, res);
    });
    svr.Post(R"(/api/recipes/(\d+)/steps/(\d+)/image)", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.uploadStepImage(req, res);
    });
    svr.Delete(R"(/api/recipes/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.deleteRecipe(req, res);
    });
}

// ============================================================
// 用户相关
// ============================================================

void Router::registerUserRoutes(httplib::Server& svr) {
    svr.Get("/api/users/me", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getCurrentUser(req, res);
    });
    svr.Put("/api/users/me/profile", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateProfile(req, res);
    });
    svr.Post("/api/users/me/avatar", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.uploadAvatar(req, res);
    });
    svr.Put("/api/users/me/password", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.changePassword(req, res);
    });
    svr.Delete("/api/users/me", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.deleteAccount(req, res);
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
    svr.Get("/api/users/me/health-profile", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getHealthProfile(req, res);
    });
    svr.Get("/api/users/me/favorites", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getFavorites(req, res);
    });
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
    svr.Patch(R"(/api/users/me/favorites/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.updateFavoriteItem(req, res);
    });
    // 批量删除收藏：注册 POST + DELETE 两个方法。
    // POST 是客户端实际使用的路径（httplib 对 DELETE 携带 body 支持不可靠）；
    // DELETE 保留供标准 REST 客户端使用。两个方法指向同一处理器。
    svr.Post("/api/users/me/favorites/batch", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.batchDeleteFavorites(req, res);
    });
    svr.Delete("/api/users/me/favorites/batch", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.batchDeleteFavorites(req, res);
    });
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
    svr.Get("/api/users/me/ratings", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getMyRatings(req, res);
    });
}

// ============================================================
// 库存管理
// ============================================================

void Router::registerInventoryRoutes(httplib::Server& svr) {
    svr.Get("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getInventory(req, res);
    });
    svr.Post("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.upsertInventory(req, res);
    });
    svr.Delete(R"(/api/inventory/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.deleteInventory(req, res);
    });
}

// ============================================================
// 购物清单
// ============================================================

void Router::registerShoppingListRoutes(httplib::Server& svr) {
    svr.Get("/api/inventory/shopping-lists", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getShoppingLists(req, res);
    });
    svr.Post("/api/inventory/shopping-lists", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.createShoppingList(req, res);
    });
    svr.Get(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getShoppingListDetail(req, res);
    });
    svr.Delete(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.deleteShoppingList(req, res);
    });
    svr.Patch(R"(/api/inventory/shopping-lists/(\d+)/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.updateShoppingListItem(req, res);
    });
    svr.Post(R"(/api/inventory/shopping-lists/(\d+)/items/batch)", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.batchAddShoppingItems(req, res);
    });
    svr.Get(R"(/api/inventory/shopping-lists/(\d+)/export)", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.exportShoppingList(req, res);
    });
}

// ============================================================
// 膳食计划
// ============================================================

void Router::registerMealPlanRoutes(httplib::Server& svr) {
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
}

// ============================================================
// 系统公告
// ============================================================

void Router::registerAnnouncementRoutes(httplib::Server& svr) {
    svr.Get("/api/announcements", [this](const httplib::Request& req, httplib::Response& res) {
        announcementHandler_.getAnnouncements(req, res);
    });
}

// ============================================================
// 管理员功能
// ============================================================

void Router::registerAdminRoutes(httplib::Server& svr) {
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
}

// ============================================================
// 公开测试接口
// ============================================================

void Router::registerPublicTestRoutes(httplib::Server& svr) {
    svr.Get("/api/inventory/public", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto conn = db_.getConnection();
            pqxx::work txn(*conn);
            pqxx::result userRes = txn.exec(
                "SELECT id FROM users WHERE username = $1", pqxx::params{"testuser"});
            if (userRes.empty()) {
                res.status = 404;
                res.set_header("Content-Type", "application/json");
                res.body = json{{"error", "Test user 'testuser' not found. Please run seed_test_data.sql"}}.dump();
                return;
            }
            int testUserId = userRes[0]["id"].as<int>();

            auto rows = txn.exec(
                "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
                "FROM inventory WHERE user_id = $1 ORDER BY added_at DESC",
                pqxx::params{testUserId});

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
        } catch (const std::exception&) {
            setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
        }
    });

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
        } catch (const std::exception&) {
            setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
        }
    });

    // 测试端点：重置 testuser 的 4 条测试通知（按 F5 调用）
    svr.Get("/api/test/reset-notifications", [this](const httplib::Request&, httplib::Response& res) {
        try {
            auto conn = db_.getConnection();
            pqxx::work txn(*conn);

            pqxx::result userRes = txn.exec(
                "SELECT id FROM users WHERE username = $1", pqxx::params{"testuser"});
            if (userRes.empty()) {
                res.status = 404;
                res.set_header("Content-Type", "application/json");
                res.body = json{{"error", "Test user 'testuser' not found"}}.dump();
                return;
            }
            int uid = userRes[0]["id"].as<int>();

            // 清空该用户的所有通知
            txn.exec("DELETE FROM notifications WHERE user_id = $1", pqxx::params{uid});

            // 重新插入 4 条测试通知
            txn.exec(
                "INSERT INTO notifications (user_id, title, content, type, sub_type, related_id, trigger_user_name, is_read) "
                "VALUES ($1, '系统维护通知', '今晚 22:00-24:00 进行系统升级，届时服务不可用。', 'system', NULL, NULL, NULL, FALSE)",
                pqxx::params{uid});
            txn.exec(
                "INSERT INTO notifications (user_id, title, content, type, sub_type, related_id, trigger_user_name, is_read) "
                "VALUES ($1, '审核结果', '您的菜谱「红烧肉」已通过审核，现在可以在首页看到啦！', 'review', NULL, 1, NULL, TRUE)",
                pqxx::params{uid});
            txn.exec(
                "INSERT INTO notifications (user_id, title, content, type, sub_type, related_id, trigger_user_name, is_read) "
                "VALUES ($1, '审核结果', '您的菜谱「清蒸鲈鱼」审核未通过，原因：图片不清晰。', 'review', NULL, 2, 'admin_cook', FALSE)",
                pqxx::params{uid});
            txn.exec(
                "INSERT INTO notifications (user_id, title, content, type, sub_type, related_id, trigger_user_name, is_read) "
                "VALUES ($1, '新的互动', '用户 foodie_lily 回复了你的评论', 'interaction', 'comment_reply', 567, 'foodie_lily', FALSE)",
                pqxx::params{uid});

            txn.commit();

            res.set_header("Content-Type", "application/json");
            res.status = 200;
            res.body = json{{"message", "4 test notifications reset"}}.dump();
        } catch (const std::exception&) {
            setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
        }
    });
}
