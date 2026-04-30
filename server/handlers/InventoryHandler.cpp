#include "InventoryHandler.h"
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>

using json = nlohmann::json;
using namespace gocook::models;

InventoryHandler::InventoryHandler(gocook::services::IInventoryService& service,
                                   AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    // ---------- 统一 Token 验证 ----------
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }

    // ---------- 解析分页参数 ----------
    int page = 1, size = 20;
    if (req.has_param("page"))
        page = std::stoi(req.get_param_value("page"));
    if (req.has_param("size"))
        size = std::stoi(req.get_param_value("size"));

    try {
        auto paged = service_.getInventory(info.userId, page, size);

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
    // ---------- 统一 Token 验证 ----------
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }

    try {
        json reqJson = json::parse(req.body);
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
    // ---------- 统一 Token 验证 ----------
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }

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
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void InventoryHandler::updateShoppingListItem(const httplib::Request& req, httplib::Response& res) {
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void InventoryHandler::batchAddShoppingItems(const httplib::Request& req, httplib::Response& res) {
    TokenInfo info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}