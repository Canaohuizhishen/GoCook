#include "InventoryHandler.h"
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include "../common/ErrorHelper.h"
#include "../common/PaginationHelper.h"
#include "../common/JsonSerializer.h"
#include "../common/Validation.h"
#include "../common/AuthHelper.h"
#include "../common/Logger.h"

using json = nlohmann::json;
using namespace gocook::models;

InventoryHandler::InventoryHandler(gocook::services::IInventoryService& service,
                                   AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    auto pp = parsePagination(req, 50);

    try {
        auto paged = service_.getInventory(info.userId, pp.page, pp.size);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = JsonSerializer::toJson(paged).dump();
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
        Validation::validateInventoryRequest(reqJson);

        UpsertInventoryRequest item;
        item.ingredient_name = reqJson["ingredient_name"];
        item.quantity = reqJson["quantity"];
        item.unit = reqJson["unit"];
        if (reqJson.contains("expiry_date"))
            item.expiry_date = reqJson["expiry_date"];

        int newId = service_.upsertInventory(info.userId, item);
        res.status = 201;
        res.body = json{{"message", "库存已更新"}, {"id", newId}}.dump();
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
    } catch (const std::exception&) {
        setErrorResponse(res, 400, "item_id 格式无效");
        return;
    }
    try {
        service_.deleteInventoryItem(info.userId, itemId);
        res.status = 200;
        res.body = json{{"message", "库存项已删除"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void InventoryHandler::getShoppingLists(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto lists = service_.getShoppingLists(info.userId);
        json arr = json::array();
        for (const auto& summary : lists)
            arr.push_back(JsonSerializer::toJson(summary));
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
        auto list = service_.getShoppingListDetail(info.userId, listId);
        res.status = 201;
        res.body = JsonSerializer::toJson(list).dump();
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
        res.body = JsonSerializer::toJson(list).dump();
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
        json reqJson = json::parse(req.body);
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
        res.body = JsonSerializer::toJson(response).dump();
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
