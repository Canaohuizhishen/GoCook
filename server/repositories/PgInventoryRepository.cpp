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

std::vector<ShoppingListSummary> PgInventoryRepository::findShoppingLists(int) {
    throw ServiceException("Not implemented", 501);
}

int PgInventoryRepository::createShoppingList(int, const CreateShoppingListRequest&) {
    throw ServiceException("Not implemented", 501);
}

ShoppingList PgInventoryRepository::findShoppingListDetail(int, int) {
    throw ServiceException("Not implemented", 501);
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
