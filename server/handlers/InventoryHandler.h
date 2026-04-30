#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>          // 依赖抽象 IInventoryService
#include <nlohmann/json.hpp>
#include "../auth_middleware.h"

class InventoryHandler {
public:
    explicit InventoryHandler(gocook::services::IInventoryService& service,
                              AuthMiddleware& auth);

    // 获取当前用户的库存（需要 token 验证，分页）
    void getInventory(const httplib::Request& req, httplib::Response& res);
    // 添加或更新库存项（需要 token 验证）
    void upsertInventory(const httplib::Request& req, httplib::Response& res);
    // 删除库存项（需要 token 验证，使用路径中的 item_id）
    void deleteInventory(const httplib::Request& req, httplib::Response& res);

    // 获取/生成购物清单（需 token 验证）
    void getShoppingList(const httplib::Request& req, httplib::Response& res);
    // 更新购物清单项状态（需 token 验证）
    void updateShoppingListItem(const httplib::Request& req, httplib::Response& res);
    // 批量添加购物清单项（需 token 验证）
    void batchAddShoppingItems(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IInventoryService& service_;   // 业务抽象
    AuthMiddleware& auth_;                           // 认证中间件
};