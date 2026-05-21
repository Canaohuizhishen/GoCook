#include "PgInventoryRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"

using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

PagedInventory PgInventoryRepository::findInventory(int userId, int page, int size) {
    PagedInventory result;
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result countRes = txn.exec_params(
            "SELECT COUNT(*) FROM inventory WHERE user_id = $1", userId);
        int total = countRes[0][0].as<int>();

        int offset = (page > 0) ? (page - 1) * size : 0;

        pqxx::result rows = txn.exec_params(
            "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
            "FROM inventory WHERE user_id = $1 "
            "ORDER BY added_at DESC LIMIT $2 OFFSET $3",
            userId, size, offset);

        for (const auto& row : rows) {
            InventoryItem item;
            item.id              = row["id"].as<int>();
            item.ingredient_name = row["ingredient_name"].c_str();
            item.quantity        = row["quantity"].as<double>();
            item.unit            = row["unit"].c_str();
            if (!row["expiry_date"].is_null())
                item.expiry_date = row["expiry_date"].c_str();
            item.added_at        = row["added_at"].c_str();
            result.data.push_back(item);
        }

        result.pagination.page        = page;
        result.pagination.size        = size;
        result.pagination.total       = total;
        result.pagination.total_pages = (total + size - 1) / size;

        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
    return result;
}

int PgInventoryRepository::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result existing = txn.exec_params(
            "SELECT id FROM inventory WHERE user_id = $1 AND ingredient_name = $2",
            userId, item.ingredient_name);

        if (!existing.empty()) {
            int existingId = existing[0]["id"].as<int>();
            if (item.expiry_date.has_value()) {
                txn.exec_params(
                    "UPDATE inventory SET quantity = $1, unit = $2, expiry_date = $3, added_at = NOW() WHERE id = $4",
                    item.quantity, item.unit, item.expiry_date.value(), existingId);
            } else {
                txn.exec_params(
                    "UPDATE inventory SET quantity = $1, unit = $2, added_at = NOW() WHERE id = $3",
                    item.quantity, item.unit, existingId);
            }
            txn.commit();
            return existingId;
        } else {
            if (item.expiry_date.has_value()) {
                pqxx::result res = txn.exec_params(
                    "INSERT INTO inventory (user_id, ingredient_name, quantity, unit, expiry_date) "
                    "VALUES ($1, $2, $3, $4, $5) RETURNING id",
                    userId, item.ingredient_name, item.quantity, item.unit, item.expiry_date.value());
                txn.commit();
                return res[0][0].as<int>();
            } else {
                pqxx::result res = txn.exec_params(
                    "INSERT INTO inventory (user_id, ingredient_name, quantity, unit) "
                    "VALUES ($1, $2, $3, $4) RETURNING id",
                    userId, item.ingredient_name, item.quantity, item.unit);
                txn.commit();
                return res[0][0].as<int>();
            }
        }
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgInventoryRepository::deleteInventoryItem(int userId, int itemId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        auto res = txn.exec_params(
            "DELETE FROM inventory WHERE id = $1 AND user_id = $2",
            itemId, userId);
        if (res.affected_rows() == 0) {
            throw ServiceException("Item not found or not owned by user", 404);
        }
        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::vector<ShoppingListSummary> PgInventoryRepository::findShoppingLists(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result rows = txn.exec_params(
            "SELECT sl.id, sl.name, COUNT(sli.id) AS item_count, sl.created_at "
            "FROM shopping_lists sl "
            "LEFT JOIN shopping_list_items sli ON sli.list_id = sl.id "
            "WHERE sl.user_id = $1 "
            "GROUP BY sl.id ORDER BY sl.created_at DESC",
            userId);

        std::vector<ShoppingListSummary> result;
        for (const auto& row : rows) {
            ShoppingListSummary summary;
            summary.id = row["id"].as<int>();
            summary.name = row["name"].c_str();
            summary.item_count = row["item_count"].as<int>();
            summary.created_at = row["created_at"].c_str();
            result.push_back(std::move(summary));
        }

        txn.commit();
        return result;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

int PgInventoryRepository::createShoppingList(int userId, const CreateShoppingListRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result res;
        if (req.plan_id.has_value()) {
            res = txn.exec_params(
                "INSERT INTO shopping_lists (user_id, name, plan_id) VALUES ($1, $2, $3) RETURNING id",
                userId, req.name, req.plan_id.value());
        } else {
            res = txn.exec_params(
                "INSERT INTO shopping_lists (user_id, name) VALUES ($1, $2) RETURNING id",
                userId, req.name);
        }
        int id = res[0]["id"].as<int>();
        txn.commit();
        return id;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

ShoppingList PgInventoryRepository::findShoppingListDetail(int userId, int listId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result listRes = txn.exec_params(
            "SELECT id, name FROM shopping_lists WHERE id = $1 AND user_id = $2",
            listId, userId);
        if (listRes.empty()) {
            throw ServiceException("购物清单不存在", 404);
        }

        ShoppingList result;
        result.id = listRes[0]["id"].as<int>();
        result.name = listRes[0]["name"].c_str();

        pqxx::result itemsRes = txn.exec_params(
            "SELECT id, ingredient_name, required_quantity, inventory_quantity, "
            "to_buy_quantity, unit, checked "
            "FROM shopping_list_items WHERE list_id = $1 ORDER BY id",
            listId);

        for (const auto& row : itemsRes) {
            ShoppingListItem item;
            item.id = row["id"].as<int>();
            item.ingredient_name = row["ingredient_name"].c_str();
            item.required_quantity = row["required_quantity"].as<double>();
            item.inventory_quantity = row["inventory_quantity"].as<double>();
            item.to_buy_quantity = row["to_buy_quantity"].as<double>();
            item.unit = row["unit"].c_str();
            item.checked = row["checked"].as<bool>();
            result.items.push_back(std::move(item));
        }

        txn.commit();
        return result;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgInventoryRepository::deleteShoppingList(int, int) {
    throw ServiceException("Not implemented", 501);
}

void PgInventoryRepository::updateShoppingListItem(int, int, int, const UpdateShoppingItemRequest&) {
    throw ServiceException("Not implemented", 501);
}

BatchShoppingResponse PgInventoryRepository::batchAddShoppingItems(int, int, const std::vector<BatchShoppingItem>&) {
    throw ServiceException("Not implemented", 501);
}

std::string PgInventoryRepository::exportShoppingList(int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
