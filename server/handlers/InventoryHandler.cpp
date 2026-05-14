#include "InventoryHandler.h"
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include "../common/ErrorHelper.h"
#include "../common/PaginationHelper.h"
#include "../common/SerializationHelper.h"
#include "../common/AuthHelper.h"

using json = nlohmann::json;
using namespace gocook::models;

// ========== 辅助序列化 ==========

static json toJson(const InventoryItem& item) {
    json obj;
    obj["id"] = item.id;
    obj["ingredient_name"] = item.ingredient_name;
    obj["quantity"] = item.quantity;
    obj["unit"] = item.unit;
    if (item.expiry_date.has_value())
        obj["expiry_date"] = item.expiry_date.value();
    obj["added_at"] = item.added_at;
    return obj;
}

static json toJson(const ShoppingListItem& item) {
    return {
        {"id", item.id},
        {"ingredient_name", item.ingredient_name},
        {"required_quantity", item.required_quantity},
        {"inventory_quantity", item.inventory_quantity},
        {"to_buy_quantity", item.to_buy_quantity},
        {"unit", item.unit},
        {"checked", item.checked}
    };
}

static json toJson(const ShoppingListSummary& summary) {
    return {
        {"id", summary.id},
        {"name", summary.name},
        {"item_count", summary.item_count},
        {"created_at", summary.created_at}
    };
}

static json toJson(const ShoppingList& list) {
    json obj;
    obj["id"] = list.id;
    obj["name"] = list.name;
    json items = json::array();
    for (const auto& item : list.items)
        items.push_back(toJson(item));
    obj["items"] = items;
    return obj;
}

static json toJson(const BatchShoppingResponse& resp) {
    json obj;
    obj["message"] = resp.message;
    obj["items"] = json::array();
    for (const auto& item : resp.items)
        obj["items"].push_back(toJson(item));
    return obj;
}

// ---------- InventoryHandler 实现 ----------

InventoryHandler::InventoryHandler(gocook::services::IInventoryService& service,
                                   AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    auto pp = parsePagination(req, 50);

    try {
        auto paged = service_.getInventory(info.userId, pp.page, pp.size);
        json resp;
        resp["data"] = json::array();
        for (const auto& item : paged.data)
            resp["data"].push_back(toJson(item));
        resp["pagination"] = toJson(paged.pagination);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::upsertInventory(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("ingredient_name") || !reqJson.contains("quantity") || !reqJson.contains("unit")) {
            setErrorResponse(res, 400, "缺少必填字段: ingredient_name, quantity, unit");
            return;
        }
        UpsertInventoryRequest item;
        item.ingredient_name = reqJson["ingredient_name"];
        item.quantity = reqJson["quantity"];
        item.unit = reqJson["unit"];
        if (reqJson.contains("expiry_date"))
            item.expiry_date = reqJson["expiry_date"];

        int newId = service_.upsertInventory(info.userId, item);
        res.status = 200;
        res.body = json{{"message", "Inventory updated successfully"}, {"id", newId}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::deleteInventory(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    if (req.matches.size() < 2) {
        setErrorResponse(res, 400, "缺少 item_id 路径参数");
        return;
    }
    int itemId;
    try {
        itemId = std::stoi(req.matches[1]);
    } catch (...) {
        setErrorResponse(res, 400, "item_id 格式无效");
        return;
    }
    try {
        service_.deleteInventoryItem(info.userId, itemId);
        res.status = 200;
        res.body = json{{"message", "Inventory item deleted"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

// ========== 购物清单（多清单模型） ==========

void InventoryHandler::getShoppingLists(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto lists = service_.getShoppingLists(info.userId);
        json arr = json::array();
        for (const auto& summary : lists)
            arr.push_back(toJson(summary));
        res.status = 200;
        res.body = arr.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::createShoppingList(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        CreateShoppingListRequest request;
        request.name = reqJson.at("name");
        if (reqJson.contains("plan_id")) request.plan_id = reqJson["plan_id"].get<std::string>();
        int listId = service_.createShoppingList(info.userId, request);
        // 返回创建后的详细信息，需要再次获取
        auto list = service_.getShoppingListDetail(info.userId, listId);
        res.status = 201;
        res.body = toJson(list).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::getShoppingListDetail(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int listId = std::stoi(req.matches[1]);
        auto list = service_.getShoppingListDetail(info.userId, listId);
        res.status = 200;
        res.body = toJson(list).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::deleteShoppingList(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int listId = std::stoi(req.matches[1]);
        service_.deleteShoppingList(info.userId, listId);
        res.status = 200;
        res.body = json{{"message", "购物清单已删除"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::updateShoppingListItem(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        // 路由: /api/inventory/shopping-lists/:list_id/items/:item_id
        int listId = std::stoi(req.matches[1]);
        int itemId = std::stoi(req.matches[2]);
        json reqJson = json::parse(req.body);
        UpdateShoppingItemRequest request;
        if (reqJson.contains("checked")) request.checked = reqJson["checked"].get<bool>();
        service_.updateShoppingListItem(info.userId, listId, itemId, request);
        res.status = 200;
        res.body = json{{"message", "清单项已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::batchAddShoppingItems(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int listId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);  // 期望是数组
        std::vector<BatchShoppingItem> items;
        for (const auto& elem : reqJson) {
            BatchShoppingItem item;
            item.ingredient_name = elem.at("ingredient_name");
            item.quantity = elem.at("quantity");
            if (elem.contains("unit")) item.unit = elem["unit"];
            items.push_back(item);
        }
        auto response = service_.batchAddShoppingItems(info.userId, listId, items);
        res.status = 201;
        res.body = toJson(response).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::exportShoppingList(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int listId = std::stoi(req.matches[1]);
        std::string format = req.has_param("format") ? req.get_param_value("format") : "text";
        std::string content = service_.exportShoppingList(info.userId, listId, format);
        if (format == "text") {
            res.set_header("Content-Type", "text/plain; charset=utf-8");
        } else {
            res.set_header("Content-Type", "image/png");
        }
        res.status = 200;
        res.body = content;
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}