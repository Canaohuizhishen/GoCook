#pragma once

#include "./third_party/httplib/httplib.h"
#include "DBConnection.h"
#include "RecipeHandler.h"
#include "UserHandler.h"
#include "InventoryHandler.h"
// 后续添加其他 Handler 的头文件

class Router {
public:
    // 构造函数接收所有 Handler 的引用（或指针）
    Router(DBConnection& db, RecipeHandler& recipeHandler,
           UserHandler& userHandler,
           InventoryHandler& inventoryHandler /*, 其他Handler... */);

    // 注册所有路由到 server 对象
    void setupRoutes(httplib::Server& svr);

private:
    DBConnection& db_;
    RecipeHandler& recipeHandler_;
    UserHandler& userHandler_;
    InventoryHandler& inventoryHandler_;
    // 后续：XXHandler& xxHandler_; 等
};