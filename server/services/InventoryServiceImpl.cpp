#include "InventoryServiceImpl.h"
#include <pqxx/pqxx>
#include <stdexcept>
#include <string>

using namespace gocook::models;
using namespace gocook::services;

// ------------------ IInventoryService 接口实现 ------------------

PagedInventory InventoryServiceImpl::getInventory(int userId, int page, int size) {
    PagedInventory result;
    try {
        pqxx::work txn(db_.getConn());

        // 计算总条数
        pqxx::result countRes = txn.exec(
            "SELECT COUNT(*) FROM inventory WHERE user_id = " + txn.quote(userId));
        int total = countRes[0][0].as<int>();

        // 分页计算
        int offset = (page > 0) ? (page - 1) * size : 0;

        // 查询数据
        pqxx::result rows = txn.exec(
            "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
            "FROM inventory WHERE user_id = " + txn.quote(userId) +
            " ORDER BY added_at DESC LIMIT " + txn.quote(size) + " OFFSET " + txn.quote(offset));

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
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
    return result;
}

int InventoryServiceImpl::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    try {
        pqxx::work txn(db_.getConn());

        // 检查冲突，若存在则更新并返回 id
        pqxx::result existing = txn.exec(
            "SELECT id FROM inventory WHERE user_id = " + txn.quote(userId) +
            " AND ingredient_name = " + txn.quote(item.ingredient_name));

        if (!existing.empty()) {
            // 更新已有记录
            std::string sql = "UPDATE inventory SET quantity = " + txn.quote(item.quantity) +
                              ", unit = " + txn.quote(item.unit) +
                              ", added_at = NOW()";
            if (item.expiry_date.has_value())
                sql += ", expiry_date = " + txn.quote(item.expiry_date.value());
            sql += " WHERE id = " + txn.quote(existing[0]["id"].as<int>());
            txn.exec(sql);
            txn.commit();
            return existing[0]["id"].as<int>();
        } else {
            // 插入新记录
            std::string insert = "INSERT INTO inventory (user_id, ingredient_name, quantity, unit";
            std::string values = "VALUES (" + txn.quote(userId) + ", " + txn.quote(item.ingredient_name) +
                                 ", " + txn.quote(item.quantity) + ", " + txn.quote(item.unit);
            if (item.expiry_date.has_value()) {
                insert += ", expiry_date";
                values += ", " + txn.quote(item.expiry_date.value());
            }
            insert += ") ";
            values += ")";
            std::string full = insert + values + " RETURNING id";
            pqxx::result res = txn.exec(full);
            txn.commit();
            return res[0][0].as<int>();
        }
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}

void InventoryServiceImpl::deleteInventoryItem(int userId, int itemId) {
    try {
        pqxx::work txn(db_.getConn());
        // 仅允许删除自己的库存项
        auto res = txn.exec(
            "DELETE FROM inventory WHERE id = " + txn.quote(itemId) +
            " AND user_id = " + txn.quote(userId));
        if (res.affected_rows() == 0) {
            throw ServiceException("Item not found or not owned by user");
        }
        txn.commit();
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}