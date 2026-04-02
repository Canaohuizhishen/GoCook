#include "InventoryHandler.h"
#include "auth_utils.h"
#include <pqxx/pqxx>

using json = nlohmann::json;

InventoryHandler::InventoryHandler(DBConnection& db) : db_(db) {}

void InventoryHandler::getInventory(const httplib::Request& req, httplib::Response& res) {
    // 从请求头获取 token
    auto auth = req.get_header_value("Authorization");
    if (auth.empty() || auth.find("Bearer ") != 0) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid Authorization header"}}.dump();
        return;
    }
    std::string token = auth.substr(7); // 去掉 "Bearer "
    TokenInfo info = verifyToken(token);
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Invalid token"}}.dump();
        return;
    }

    try {
        pqxx::work txn(db_.getConn());
        auto result = txn.exec_params(
            "SELECT ingredient_name, quantity, unit, expiry_date, added_at FROM inventory WHERE user_id = $1",
            info.userId);
        json inventory = json::array();
        for (const auto& row : result) {
            json item;
            item["ingredient_name"] = row["ingredient_name"].as<std::string>();
            item["quantity"] = row["quantity"].as<double>();
            item["unit"] = row["unit"].as<std::string>();
            if (!row["expiry_date"].is_null())
                item["expiry_date"] = row["expiry_date"].as<std::string>();
            item["added_at"] = row["added_at"].as<std::string>();
            inventory.push_back(item);
        }
        txn.commit();
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = inventory.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::upsertInventory(const httplib::Request& req, httplib::Response& res) {
    // 验证 token（同上，略作简化，实际应抽取公共函数）
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
        // 必需字段: ingredient_name, quantity, unit
        if (!reqJson.contains("ingredient_name") || !reqJson.contains("quantity") || !reqJson.contains("unit")) {
            res.status = 400;
            res.body = json{{"error", "Missing required fields: ingredient_name, quantity, unit"}}.dump();
            return;
        }
        std::string ingredient = reqJson["ingredient_name"];
        double quantity = reqJson["quantity"];
        std::string unit = reqJson["unit"];
        std::string expiry_date = reqJson.value("expiry_date", "");

        pqxx::work txn(db_.getConn());
        // 使用 ON CONFLICT 更新（需要 inventory 表有主键 (user_id, ingredient_name)）
        if (expiry_date.empty()) {
            txn.exec_params(
                "INSERT INTO inventory (user_id, ingredient_name, quantity, unit) VALUES ($1, $2, $3, $4) "
                "ON CONFLICT (user_id, ingredient_name) DO UPDATE SET quantity = EXCLUDED.quantity, unit = EXCLUDED.unit, added_at = NOW()",
                info.userId, ingredient, quantity, unit);
        } else {
            txn.exec_params(
                "INSERT INTO inventory (user_id, ingredient_name, quantity, unit, expiry_date) VALUES ($1, $2, $3, $4, $5) "
                "ON CONFLICT (user_id, ingredient_name) DO UPDATE SET quantity = EXCLUDED.quantity, unit = EXCLUDED.unit, expiry_date = EXCLUDED.expiry_date, added_at = NOW()",
                info.userId, ingredient, quantity, unit, expiry_date);
        }
        txn.commit();
        res.status = 200;
        res.body = json{{"message", "Inventory updated successfully"}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::deleteInventory(const httplib::Request& req, httplib::Response& res) {
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
        if (!reqJson.contains("ingredient_name")) {
            res.status = 400;
            res.body = json{{"error", "Missing ingredient_name"}}.dump();
            return;
        }
        std::string ingredient = reqJson["ingredient_name"];

        pqxx::work txn(db_.getConn());
        txn.exec_params("DELETE FROM inventory WHERE user_id = $1 AND ingredient_name = $2", info.userId, ingredient);
        txn.commit();
        res.status = 200;
        res.body = json{{"message", "Inventory item deleted"}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void InventoryHandler::getInventoryPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        pqxx::work txn(db_.getConn());
        // 返回测试用户 'testuser' 的库存
        // 先根据用户名查找 user_id
        pqxx::result userRes = txn.exec_params("SELECT id FROM users WHERE username = $1", "testuser");
        if (userRes.empty()) {
            res.status = 404;
            res.body = json{{"error", "Test user 'testuser' not found. Please run seed_test_data.sql"}}.dump();
            return;
        }
        int testUserId = userRes[0]["id"].as<int>();

        auto result = txn.exec_params(
            "SELECT ingredient_name, quantity, unit, expiry_date, added_at FROM inventory WHERE user_id = $1",
            testUserId);

        json inventory = json::array();
        for (const auto& row : result) {
            json item;
            item["ingredient_name"] = row["ingredient_name"].as<std::string>();
            item["quantity"] = row["quantity"].as<double>();
            item["unit"] = row["unit"].as<std::string>();
            if (!row["expiry_date"].is_null())
                item["expiry_date"] = row["expiry_date"].as<std::string>();
            item["added_at"] = row["added_at"].as<std::string>();
            inventory.push_back(item);
        }
        txn.commit();

        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = inventory.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}