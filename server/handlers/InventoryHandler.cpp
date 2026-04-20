// InventoryHandler.cpp
#include "InventoryHandler.h"
#include "../auth_utils.h"
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

InventoryHandler::InventoryHandler(gocook::services::IInventoryService& service)
    : service_(service) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    // ---------- Token 验证 ----------
    auto auth = req.get_header_value("Authorization");
    if (auth.empty() || auth.find("Bearer ") != 0) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid Authorization header"}}.dump();
        return;
    }
    std::string token = auth.substr(7);  // 去掉 "Bearer "
    TokenInfo info = verifyToken(token);
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Invalid token"}}.dump();
        return;
    }

    // ---------- 解析分页参数 ----------
    int page = 1, size = 20;
    if (req.has_param("page"))
        page = std::stoi(req.get_param_value("page"));
    if (req.has_param("size"))
        size = std::stoi(req.get_param_value("size"));

    try {
        // 调用抽象服务，获得分页结果
        auto paged = service_.getInventory(info.userId, page, size);

        // 序列化为 API 规范格式
        json resp;
        resp["data"] = json::array();
        for (const auto& item : paged.data) {
            json obj;
            obj["id"]               = item.id;
            obj["ingredient_name"]  = item.ingredient_name;
            obj["quantity"]         = item.quantity;
            obj["unit"]             = item.unit;
            if (item.expiry_date.has_value())
                obj["expiry_date"] = item.expiry_date.value();
            obj["added_at"]         = item.added_at;
            resp["data"].push_back(obj);
        }
        resp["pagination"] = {
            {"page", paged.pagination.page},
            {"size", paged.pagination.size},
            {"total", paged.pagination.total},
            {"total_pages", paged.pagination.total_pages}
        };

        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::upsertInventory(const httplib::Request& req, httplib::Response& res) {
    // ---------- Token 验证 ----------
    auto auth = req.get_header_value("Authorization");
    if (auth.empty() || auth.find("Bearer ") != 0) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid Authorization header"}}.dump();
        return;
    }
    std::string token = auth.substr(7);
    TokenInfo info = verifyToken(token);
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Invalid token"}}.dump();
        return;
    }

    try {
        json reqJson = json::parse(req.body);
        // 必需字段检查
        if (!reqJson.contains("ingredient_name") || !reqJson.contains("quantity") || !reqJson.contains("unit")) {
            res.status = 400;
            res.body = json{{"error", "Missing required fields: ingredient_name, quantity, unit"}}.dump();
            return;
        }

        gocook::models::UpsertInventoryRequest item;
        item.ingredient_name = reqJson["ingredient_name"];
        item.quantity        = reqJson["quantity"];
        item.unit            = reqJson["unit"];
        if (reqJson.contains("expiry_date"))
            item.expiry_date = reqJson["expiry_date"];

        // 委托给 Service
        int newId = service_.upsertInventory(info.userId, item);

        res.status = 200;
        res.body = json{{"message", "Inventory updated successfully"}, {"id", newId}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::deleteInventory(const httplib::Request& req, httplib::Response& res) {
    // ---------- Token 验证 ----------
    auto auth = req.get_header_value("Authorization");
    if (auth.empty() || auth.find("Bearer ") != 0) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid Authorization header"}}.dump();
        return;
    }
    std::string token = auth.substr(7);
    TokenInfo info = verifyToken(token);
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Invalid token"}}.dump();
        return;
    }

    // ---------- 从路径提取 item_id ----------
    // 路由注册时需使用正则捕获，例如：/api/inventory/(\d+)
    // 这里的 req.matches[1] 即为捕获到的 item_id 字符串
    if (req.matches.size() < 2) {
        res.status = 400;
        res.body = json{{"error", "Missing item_id in path"}}.dump();
        return;
    }
    int itemId;
    try {
        itemId = std::stoi(req.matches[1]);
    } catch (...) {
        res.status = 400;
        res.body = json{{"error", "Invalid item_id"}}.dump();
        return;
    }

    try {
        service_.deleteInventoryItem(info.userId, itemId);
        res.status = 200;
        res.body = json{{"message", "Inventory item deleted"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::getShoppingList(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void InventoryHandler::updateShoppingListItem(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void InventoryHandler::batchAddShoppingItems(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}