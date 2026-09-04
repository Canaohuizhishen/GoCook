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

namespace {
    // 关键字两侧去空白：ASCII 空白 + 全角空格 U+3000（UTF-8: E3 80 80），
    // 与客户端 QString::trimmed() 的空白集合对齐（纯空白 → 空串 = 不过滤）
    std::string trimKeyword(const std::string& s) {
        size_t b = 0, e = s.size();
        while (b < e) {
            const unsigned char c = static_cast<unsigned char>(s[b]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { ++b; continue; }
            if (c == 0xE3 && b + 2 < e && static_cast<unsigned char>(s[b + 1]) == 0x80
                && static_cast<unsigned char>(s[b + 2]) == 0x80) { b += 3; continue; }
            break;
        }
        while (e > b) {
            const unsigned char c = static_cast<unsigned char>(s[e - 1]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { --e; continue; }
            if (c == 0x80 && e >= 3 && static_cast<unsigned char>(s[e - 2]) == 0x80
                && static_cast<unsigned char>(s[e - 3]) == 0xE3) { e -= 3; continue; }
            break;
        }
        return s.substr(b, e - b);
    }
}

InventoryHandler::InventoryHandler(gocook::services::IInventoryService& service,
                                   AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    auto pp = parsePagination(req, 50);

    try {
        // 食材名模糊过滤（v2.14 keyword；v2.15 服务端收口）：可选查询参数。
        // 统一 trim（ASCII 空白 + 全角空格，与客户端 QString::trimmed() 对齐）：
        // 前后空白/纯空白视为不过滤；UTF-8 长度上限 100 字节（防超长请求滥用），超限返 400
        std::string keyword = trimKeyword(
            req.has_param("keyword") ? req.get_param_value("keyword") : std::string());
        if (keyword.size() > 100)
            throw gocook::services::ServiceException("过滤词过长（上限 100 字节）", 400);

        auto paged = service_.getInventory(info.userId, pp.page, pp.size, keyword);
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

void InventoryHandler::updateInventoryItem(const httplib::Request& req, httplib::Response& res) {
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
        json reqJson = json::parse(req.body);
        Validation::validateInventoryRequest(reqJson);

        UpsertInventoryRequest item;
        item.ingredient_name = reqJson["ingredient_name"];
        item.quantity = reqJson["quantity"];
        item.unit = reqJson["unit"];
        if (reqJson.contains("expiry_date"))
            item.expiry_date = reqJson["expiry_date"];

        service_.updateInventoryItem(info.userId, itemId, item);
        res.status = 200;
        res.body = json{{"message", "库存项已更新"}, {"id", itemId}}.dump();
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
        // 仅支持 text（v2.13）：repo 层对非 text 抛 ServiceException(400)，此处的其他分支不可达，不再设置 image/png 头
        std::string content = service_.exportShoppingList(info.userId, listId, format);
        res.set_header("Content-Type", "text/plain; charset=utf-8");
        res.status = 200;
        res.body = content;
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}
