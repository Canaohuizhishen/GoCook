#include "RecipeHandler.h"
#include "./nlohmann/json.hpp"
#include <pqxx/pqxx>
#include "auth_utils.h"

using json = nlohmann::json;

RecipeHandler::RecipeHandler(DBConnection& db) : db_(db) {}

void RecipeHandler::getRecipes(const httplib::Request& req, httplib::Response& res) {
    // 验证 token
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
        pqxx::work txn(db_.getConn());
        auto result = txn.exec("SELECT id, name, description FROM recipes LIMIT 100");
        json recipes = json::array();
        for (const auto& row : result) {
            json recipe;
            recipe["id"] = row["id"].as<int>();
            recipe["name"] = row["name"].as<std::string>();
            recipe["description"] = row["description"].as<std::string>();
            recipes.push_back(recipe);
        }
        txn.commit();
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = recipes.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void RecipeHandler::getRecipesPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        pqxx::work txn(db_.getConn());
        auto result = txn.exec("SELECT id, name, description FROM recipes LIMIT 100");
        json recipes = json::array();
        for (const auto& row : result) {
            json recipe;
            recipe["id"] = row["id"].as<int>();
            recipe["name"] = row["name"].as<std::string>();
            recipe["description"] = row["description"].as<std::string>();
            recipes.push_back(recipe);
        }
        txn.commit();
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = recipes.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}
