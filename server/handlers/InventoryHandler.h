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

    // ---------- 购物清单（多清单模型，需认证） ----------
    // 获取用户的购物清单列表
    void getShoppingLists(const httplib::Request& req, httplib::Response& res);
    // 创建购物清单
    void createShoppingList(const httplib::Request& req, httplib::Response& res);
    // 获取指定购物清单详情
    void getShoppingListDetail(const httplib::Request& req, httplib::Response& res);
    // 删除购物清单
    void deleteShoppingList(const httplib::Request& req, httplib::Response& res);
    // 更新购物清单项状态（需传入 listId 和 itemId 路径参数）
    void updateShoppingListItem(const httplib::Request& req, httplib::Response& res);
    // 批量添加购物清单项（需传入 listId 路径参数）
    void batchAddShoppingItems(const httplib::Request& req, httplib::Response& res);
    // 导出购物清单
    void exportShoppingList(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IInventoryService& service_;   // 业务抽象
    AuthMiddleware& auth_;                           // 认证中间件
};