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

        LOG_DEBUG("[SQL] findInventory count | userId=%d", userId);
        pqxx::result countRes = txn.exec(
            "SELECT COUNT(*) FROM inventory WHERE user_id = $1", pqxx::params{userId});
        int total = countRes[0][0].as<int>();

        int offset = (page > 0) ? (page - 1) * size : 0;

        LOG_DEBUG("[SQL] findInventory data | userId=%d page=%d size=%d", userId, page, size);
        pqxx::result rows = txn.exec(
            "SELECT id, ingredient_name, quantity, unit, expiry_date, added_at "
            "FROM inventory WHERE user_id = $1 "
            "ORDER BY added_at DESC LIMIT $2 OFFSET $3",
            pqxx::params{userId, size, offset});

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
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
    return result;
}

int PgInventoryRepository::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        LOG_DEBUG("[SQL] upsertInventory check existing | userId=%d ing=%s", userId, item.ingredient_name.c_str());
        pqxx::result existing = txn.exec(
            "SELECT id FROM inventory WHERE user_id = $1 AND ingredient_name = $2",
            pqxx::params{userId, item.ingredient_name});

        if (!existing.empty()) {
            int existingId = existing[0]["id"].as<int>();
            if (item.expiry_date.has_value()) {
                LOG_DEBUG("[SQL] upsertInventory UPDATE (with expiry) | id=%d", existingId);
                txn.exec(
                    "UPDATE inventory SET quantity = $1, unit = $2, expiry_date = $3, added_at = NOW() WHERE id = $4",
                    pqxx::params{item.quantity, item.unit, item.expiry_date.value(), existingId});
            } else {
                LOG_DEBUG("[SQL] upsertInventory UPDATE (no expiry) | id=%d", existingId);
                txn.exec(
                    "UPDATE inventory SET quantity = $1, unit = $2, added_at = NOW() WHERE id = $3",
                    pqxx::params{item.quantity, item.unit, existingId});
            }
            txn.commit();
            return existingId;
        } else {
            if (item.expiry_date.has_value()) {
                LOG_DEBUG("[SQL] upsertInventory INSERT (with expiry) | userId=%d ing=%s", userId, item.ingredient_name.c_str());
                pqxx::result res = txn.exec(
                    "INSERT INTO inventory (user_id, ingredient_name, quantity, unit, expiry_date) "
                    "VALUES ($1, $2, $3, $4, $5) RETURNING id",
                    pqxx::params{userId, item.ingredient_name, item.quantity, item.unit, item.expiry_date.value()});
                txn.commit();
                return res[0][0].as<int>();
            } else {
                LOG_DEBUG("[SQL] upsertInventory INSERT (no expiry) | userId=%d ing=%s", userId, item.ingredient_name.c_str());
                pqxx::result res = txn.exec(
                    "INSERT INTO inventory (user_id, ingredient_name, quantity, unit) "
                    "VALUES ($1, $2, $3, $4) RETURNING id",
                    pqxx::params{userId, item.ingredient_name, item.quantity, item.unit});
                txn.commit();
                return res[0][0].as<int>();
            }
        }
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgInventoryRepository::deleteInventoryItem(int userId, int itemId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] deleteInventoryItem | id=%d userId=%d", itemId, userId);
        auto res = txn.exec(
            "DELETE FROM inventory WHERE id = $1 AND user_id = $2",
            pqxx::params{itemId, userId});
        if (res.affected_rows() == 0) {
            throw ServiceException("Item not found or not owned by user", 404);
        }
        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::vector<ShoppingListSummary> PgInventoryRepository::findShoppingLists(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result rows = txn.exec(
            "SELECT sl.id, sl.name, COUNT(sli.id) AS item_count, sl.created_at "
            "FROM shopping_lists sl "
            "LEFT JOIN shopping_list_items sli ON sli.list_id = sl.id "
            "WHERE sl.user_id = $1 "
            "GROUP BY sl.id ORDER BY sl.created_at DESC",
            pqxx::params{userId});

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
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

int PgInventoryRepository::createShoppingList(int userId, const CreateShoppingListRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result res;
        if (req.plan_id.has_value()) {
            res = txn.exec(
                "INSERT INTO shopping_lists (user_id, name, plan_id) VALUES ($1, $2, $3) RETURNING id",
                pqxx::params{userId, req.name, req.plan_id.value()});
        } else {
            res = txn.exec(
                "INSERT INTO shopping_lists (user_id, name) VALUES ($1, $2) RETURNING id",
                pqxx::params{userId, req.name});
        }
        int id = res[0]["id"].as<int>();
        txn.commit();
        return id;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

ShoppingList PgInventoryRepository::findShoppingListDetail(int userId, int listId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result listRes = txn.exec(
            "SELECT id, name FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});
        if (listRes.empty()) {
            throw ServiceException("购物清单不存在", 404);
        }

        ShoppingList result;
        result.id = listRes[0]["id"].as<int>();
        result.name = listRes[0]["name"].c_str();

        pqxx::result itemsRes = txn.exec(
            "SELECT id, ingredient_name, required_quantity, inventory_quantity, "
            "to_buy_quantity, unit, checked "
            "FROM shopping_list_items WHERE list_id = $1 ORDER BY id",
            pqxx::params{listId});

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
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgInventoryRepository::deleteShoppingList(int userId, int listId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result res = txn.exec(
            "DELETE FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});

        if (res.affected_rows() == 0) {
            throw ServiceException("购物清单不存在", 404);
        }

        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgInventoryRepository::updateShoppingListItem(int userId, int listId, int itemId,
                                                       const UpdateShoppingItemRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // 查询当前清单项 + 验证归属
        pqxx::result itemRes = txn.exec(
            "SELECT sli.checked, sli.ingredient_name, sli.to_buy_quantity, sli.unit "
            "FROM shopping_list_items sli "
            "JOIN shopping_lists sl ON sl.id = sli.list_id "
            "WHERE sli.id = $1 AND sl.id = $2 AND sl.user_id = $3",
            pqxx::params{itemId, listId, userId});

        if (itemRes.empty()) {
            throw ServiceException("清单项不存在", 404);
        }

        bool oldChecked = itemRes[0]["checked"].as<bool>();
        std::string ingredientName = itemRes[0]["ingredient_name"].c_str();
        double toBuyQty = itemRes[0]["to_buy_quantity"].as<double>();
        std::string unit = itemRes[0]["unit"].c_str();

        // 更新 checked 状态
        txn.exec(
            "UPDATE shopping_list_items SET checked = $1 WHERE id = $2",
            pqxx::params{req.checked, itemId});

        // 库存回流：checked 从 false → true 时触发
        if (!oldChecked && req.checked && toBuyQty > 0) {
            // 按 user_id + ingredient_name 查找（与 upsertInventory 一致，
            // inventory 表的 UNIQUE 约束为 (user_id, ingredient_name)）
            pqxx::result existing = txn.exec(
                "SELECT id, quantity FROM inventory "
                "WHERE user_id = $1 AND ingredient_name = $2",
                pqxx::params{userId, ingredientName});

            if (!existing.empty()) {
                int invId = existing[0]["id"].as<int>();
                double currentQty = existing[0]["quantity"].as<double>();
                txn.exec(
                    "UPDATE inventory SET quantity = $1, unit = $2, added_at = NOW() WHERE id = $3",
                    pqxx::params{currentQty + toBuyQty, unit, invId});
            } else {
                // 全新食材，INSERT
                txn.exec(
                    "INSERT INTO inventory (user_id, ingredient_name, quantity, unit) "
                    "VALUES ($1, $2, $3, $4)",
                    pqxx::params{userId, ingredientName, toBuyQty, unit});
            }
        }

        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

BatchShoppingResponse PgInventoryRepository::batchAddShoppingItems(int userId, int listId, const std::vector<BatchShoppingItem>& items) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // 验证购物清单归属
        pqxx::result listRes = txn.exec(
            "SELECT id FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});
        if (listRes.empty()) {
            throw ServiceException("购物清单不存在", 404);
        }

        BatchShoppingResponse result;
        int addedCount = 0;

        for (const auto& reqItem : items) {
            // 查询当前库存量
            double invQty = 0.0;
            pqxx::result invRes = txn.exec(
                "SELECT quantity FROM inventory WHERE user_id = $1 AND ingredient_name = $2",
                pqxx::params{userId, reqItem.ingredient_name});
            if (!invRes.empty()) {
                invQty = invRes[0]["quantity"].as<double>();
            }

            double requiredQty = reqItem.quantity;
            double toBuyQty = std::max(requiredQty - invQty, 0.0);

            pqxx::result insertRes = txn.exec(
                "INSERT INTO shopping_list_items "
                "(list_id, ingredient_name, required_quantity, inventory_quantity, to_buy_quantity, unit, checked) "
                "VALUES ($1, $2, $3, $4, $5, $6, FALSE) RETURNING id",
                pqxx::params{listId, reqItem.ingredient_name, requiredQty, invQty, toBuyQty, reqItem.unit});

            ShoppingListItem listItem;
            listItem.id = insertRes[0]["id"].as<int>();
            listItem.ingredient_name = reqItem.ingredient_name;
            listItem.required_quantity = requiredQty;
            listItem.inventory_quantity = invQty;
            listItem.to_buy_quantity = toBuyQty;
            listItem.unit = reqItem.unit;
            listItem.checked = false;
            result.items.push_back(std::move(listItem));
            ++addedCount;
        }

        result.message = "Successfully added " + std::to_string(addedCount) + " items";

        txn.commit();
        return result;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::string PgInventoryRepository::exportShoppingList(int userId, int listId, const std::string& format) {
    if (format != "text") {
        throw ServiceException("不支持的导出格式，仅支持 text", 400);
    }
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        pqxx::result listRes = txn.exec(
            "SELECT name FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});
        if (listRes.empty()) {
            throw ServiceException("购物清单不存在", 404);
        }
        std::string listName = listRes[0]["name"].c_str();

        pqxx::result itemsRes = txn.exec(
            "SELECT ingredient_name, to_buy_quantity, unit, checked "
            "FROM shopping_list_items WHERE list_id = $1 ORDER BY id",
            pqxx::params{listId});

        std::string result;
        result += "GoCook 购物清单：" + listName + "\n\n";

        for (const auto& row : itemsRes) {
            bool checked = row["checked"].as<bool>();
            std::string name = row["ingredient_name"].c_str();
            double qty = row["to_buy_quantity"].as<double>();
            std::string unit = row["unit"].c_str();

            // 格式化数量：去掉末尾多余的 .000000
            std::string qtyStr = std::to_string(qty);
            auto dot = qtyStr.find('.');
            if (dot != std::string::npos) {
                auto end = qtyStr.find_last_not_of('0');
                if (end > dot) qtyStr = qtyStr.substr(0, end + 1);
                else           qtyStr = qtyStr.substr(0, dot);
            }
            result += checked ? "[x] " : "[ ] ";
            result += name + "  x" + qtyStr;
            if (!unit.empty()) result += " " + unit;
            result += "\n";
        }

        txn.commit();
        return result;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}
