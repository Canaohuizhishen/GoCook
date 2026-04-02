#pragma once

#include "./third_party/httplib/httplib.h"
#include "DBConnection.h"
#include "./nlohmann/json.hpp"

class InventoryHandler {
public:
    explicit InventoryHandler(DBConnection& db);

    // 获取当前用户的库存（需要 token 验证）
    void getInventory(const httplib::Request& req, httplib::Response& res);
    // 添加或更新库存项
    void upsertInventory(const httplib::Request& req, httplib::Response& res);
    // 删除库存项
    void deleteInventory(const httplib::Request& req, httplib::Response& res);
    // 公开测试接口（无需认证）
    void getInventoryPublic(const httplib::Request& req, httplib::Response& res);

private:
    DBConnection& db_;
};