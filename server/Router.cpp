#include "Router.h"

Router::Router(DBConnection& db,
               RecipeHandler& recipeHandler,
               UserHandler& userHandler,
               InventoryHandler& inventoryHandler)
    : db_(db),
    recipeHandler_(recipeHandler),
    userHandler_(userHandler),
    inventoryHandler_(inventoryHandler) {}

void Router::setupRoutes(httplib::Server& svr) {
    // 根路径（公开）
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        // 构建一个简单的 HTML 页面，包含公共接口的超链接
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

    // 用户认证相关（公开）
    svr.Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.registerUser(req, res);
    });
    svr.Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.loginUser(req, res);
    });

    // 菜谱相关（需要认证）
    svr.Get("/api/recipes", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipes(req, res);
    });

    // 库存管理（需要认证）
    svr.Get("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getInventory(req, res);
    });
    svr.Post("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.upsertInventory(req, res);
    });
    svr.Delete("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.deleteInventory(req, res);
    });

    // 菜谱公开测试接口
    svr.Get("/api/recipes/public", [this](const httplib::Request& req, httplib::Response& res) {
        recipeHandler_.getRecipesPublic(req, res);
    });

    // 库存公开测试接口
    svr.Get("/api/inventory/public", [this](const httplib::Request& req, httplib::Response& res) {
        inventoryHandler_.getInventoryPublic(req, res);
    });

    // 用户公开测试接口
    svr.Get("/api/users/public", [this](const httplib::Request& req, httplib::Response& res) {
        userHandler_.getUsersPublic(req, res);
    });
}