#include "PgInventoryRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include "../common/DbExecutor.h"

using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

//   本文件已按"生产级收敛形态"重构：每个方法用 executeDb 包裹（见 ../common/DbExecutor.h），
//   方法体只剩"差异部分"（SQL + 参数 + 行→结构体映射），异常分层/事务边界由辅助函数统一保证。

PagedInventory PgInventoryRepository::findInventory(int userId, int page, int size) {
    // 无过滤查询 = 过滤查询的空词特例（SQL 单源，避免两段 SQL 漂移）
    return findInventoryFiltered(userId, page, size, "");
}

PagedInventory PgInventoryRepository::findInventoryFiltered(int userId, int page, int size,
                                                           const std::string& keyword) {
    return executeDb(db_, [&](pqxx::work& txn) {
        PagedInventory result;

        // 食材名模糊过滤（v2.14）：keyword 空串 = 不过滤。参数化 + 通配符转义——
        // 用户输入里的 % _ \ 先经 replace 转义再套 ILIKE '%…%'，不会被当作 LIKE 通配符；
        // count 与 data 两处条件一致，保证分页 total 与过滤结果同步
        // R"(…)" 内为 SQL 原文（反斜杠无需 C++ 级转义）；raw 串终止符是 )"，
        // SQL 以 ')' 收尾时需在末行后换行闭合，避免右括号被终止符吞掉
        const std::string kwFilter = R"( AND ($2 = '' OR ingredient_name ILIKE '%' ||
            replace(replace(replace($2, '\', '\\'), '%', '\%'), '_', '\_') ||
            '%' ESCAPE '\')
        )";

        LOG_DEBUG("[SQL] findInventory count | userId=%d keyword=%s", userId, keyword.c_str());
        pqxx::result countRes = txn.exec(
            R"(SELECT COUNT(*) FROM inventory WHERE user_id = $1)" + kwFilter,
            pqxx::params{userId, keyword});
        int total = countRes[0][0].as<int>();

        int offset = (page > 0) ? (page - 1) * size : 0;

        LOG_DEBUG("[SQL] findInventory data | userId=%d page=%d size=%d keyword=%s",
                  userId, page, size, keyword.c_str());
        pqxx::result rows = txn.exec(
            R"(SELECT id, ingredient_name, quantity, unit, expiry_date, added_at
               FROM inventory WHERE user_id = $1)" + kwFilter +
            R"( ORDER BY added_at DESC LIMIT $3 OFFSET $4)",
            pqxx::params{userId, keyword, size, offset});

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
        result.pagination.total_pages = safeTotalPages(total, size);

        return result;
    }, "数据库操作失败");
}

int PgInventoryRepository::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 单语句原子 UPSERT（决策 2026-09-04 语义）：同名同单位 → 数量累加；无同名同单位 →
        // 新增行（不同单位各自成行、互不覆盖）。唯一约束 (user_id, ingredient_name, unit)
        // 同时充当并发闸门——两请求并发添加同单位时由 ON CONFLICT 串行化，不会丢累加
        // （旧实现 SELECT 后 UPDATE/INSERT 分两趟，check-then-act 有丢失更新窗口）。
        // expiry 语义（混批安全下限）：带 expiry_date 追加取行内最早到期日（LEAST 忽略 NULL，
        // 兼容 "2026-4-2" 这类非补零输入：EXCLUDED 经列类型隐式转 date 比较）；
        // 缺省（$5 为 NULL）时 LEAST(既有, NULL) = 既有 → 不清既有日期。
        LOG_DEBUG("[SQL] upsertInventory UPSERT | userId=%d ing=%s qty=%.1f unit=%s",
                  userId, item.ingredient_name.c_str(), item.quantity, item.unit.c_str());
        pqxx::result res = txn.exec(
            "INSERT INTO inventory (user_id, ingredient_name, quantity, unit, expiry_date) "
            "VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (user_id, ingredient_name, unit) DO UPDATE SET "
            "    quantity = inventory.quantity + EXCLUDED.quantity, "
            "    expiry_date = LEAST(inventory.expiry_date, EXCLUDED.expiry_date), "
            "    added_at = NOW() "
            "RETURNING id",
            pqxx::params{userId, item.ingredient_name, item.quantity, item.unit,
                         item.expiry_date.has_value() ? item.expiry_date.value().c_str() : nullptr});
        return res[0][0].as<int>();
    }, "数据库操作失败");
}

void PgInventoryRepository::deleteInventoryItem(int userId, int itemId) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] deleteInventoryItem | id=%d userId=%d", itemId, userId);
        auto res = txn.exec(
            "DELETE FROM inventory WHERE id = $1 AND user_id = $2",
            pqxx::params{itemId, userId});
        if (res.affected_rows() == 0) {
            throw ServiceException("库存项不存在或不属于当前用户", 404);
        }
    }, "数据库操作失败");
}

void PgInventoryRepository::updateInventoryItem(int userId, int itemId,
                                                const UpsertInventoryRequest& item) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 改名/改单位可能撞上本人另一条 (user_id, ingredient_name, unit)：唯一约束冲突翻译成 409，
        // 而不是让 pqxx 异常落进 executeDb 的通用翻译变 500
        auto execUpdate = [&](const std::string& sql, pqxx::params args) {
            try {
                return txn.exec(sql, args);
            } catch (const pqxx::sql_error& e) {
                if (e.sqlstate() == "23505")
                    throw ServiceException("已存在同名同单位的库存条目，请直接编辑该条目", 409);
                throw;
            }
        };

        if (item.expiry_date.has_value()) {
            LOG_DEBUG("[SQL] updateInventoryItem (with expiry) | id=%d userId=%d", itemId, userId);
            auto res = execUpdate(
                "UPDATE inventory SET ingredient_name = $1, quantity = $2, unit = $3, "
                "expiry_date = $4, added_at = NOW() "
                "WHERE id = $5 AND user_id = $6",
                pqxx::params{item.ingredient_name, item.quantity, item.unit,
                             item.expiry_date.value(), itemId, userId});
            if (res.affected_rows() == 0) {
                throw ServiceException("库存项不存在或不属于当前用户", 404);
            }
        } else {
            LOG_DEBUG("[SQL] updateInventoryItem (no expiry) | id=%d userId=%d", itemId, userId);
            // 整行替换语义（与 POST 累加"缺省不清"显式分离）：请求缺省 expiry_date → 清空该列，
            // 否则"编辑去掉过期日期"永远无法生效
            auto res = execUpdate(
                "UPDATE inventory SET ingredient_name = $1, quantity = $2, unit = $3, "
                "expiry_date = NULL, added_at = NOW() "
                "WHERE id = $4 AND user_id = $5",
                pqxx::params{item.ingredient_name, item.quantity, item.unit, itemId, userId});
            if (res.affected_rows() == 0) {
                throw ServiceException("库存项不存在或不属于当前用户", 404);
            }
        }
    }, "数据库操作失败");
}

std::vector<ShoppingListSummary> PgInventoryRepository::findShoppingLists(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
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
        return result;
    }, "数据库操作失败");
}

int PgInventoryRepository::createShoppingList(int userId, const CreateShoppingListRequest& req) {
    return executeDb(db_, [&](pqxx::work& txn) {
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
        return res[0]["id"].as<int>();
    }, "数据库操作失败");
}

ShoppingList PgInventoryRepository::findShoppingListDetail(int userId, int listId) {
    return executeDb(db_, [&](pqxx::work& txn) {
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
        return result;
    }, "数据库操作失败");
}

void PgInventoryRepository::deleteShoppingList(int userId, int listId) {
    executeDb(db_, [&](pqxx::work& txn) {
        pqxx::result res = txn.exec(
            "DELETE FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});

        if (res.affected_rows() == 0) {
            throw ServiceException("购物清单不存在", 404);
        }
    }, "数据库操作失败");
}

void PgInventoryRepository::updateShoppingListItem(int userId, int listId, int itemId,
                                                       const UpdateShoppingItemRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
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

        // 库存回流（api-spec 5.5 / v2.8 同步细节）：checked 从 false → true 时触发。
        // 单语句原子 UPSERT：① 已有 同名同单位 条目 → 数量累加（保留该行自身单位，不做跨单位加法）；
        // ② 无同名同单位条目 → 自动新建行（沿用清单项单位），同名不同单位旧行保留（spec 5.5 第 4 条）。
        // 由唯一约束充当并发闸门，勾选与并发回流不会互相丢更新。
        if (!oldChecked && req.checked && toBuyQty > 0) {
            LOG_DEBUG("[SQL] updateShoppingListItem 库存回流 UPSERT | ing=%s qty=%.1f unit=%s",
                      ingredientName.c_str(), toBuyQty, unit.c_str());
            txn.exec(
                "INSERT INTO inventory (user_id, ingredient_name, quantity, unit) "
                "VALUES ($1, $2, $3, COALESCE($4, '克')) "
                "ON CONFLICT (user_id, ingredient_name, unit) DO UPDATE SET "
                "    quantity = inventory.quantity + EXCLUDED.quantity, "
                "    added_at = NOW()",
                pqxx::params{userId, ingredientName, toBuyQty, unit});
        }
    }, "数据库操作失败");
}

BatchShoppingResponse PgInventoryRepository::batchAddShoppingItems(int userId, int listId, const std::vector<BatchShoppingItem>& items) {
    return executeDb(db_, [&](pqxx::work& txn) {
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
            // 查询当前库存量，算出"还差多少要买"（to_buy = max(需要 - 已有, 0)）
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

        result.message = "已成功添加 " + std::to_string(addedCount) + " 项";
        return result;
    }, "数据库操作失败");
}

std::string PgInventoryRepository::exportShoppingList(int userId, int listId, const std::string& format) {
    if (format != "text") {
        throw ServiceException("不支持的导出格式，仅支持 text", 400);   // 纯参数校验，不碰数据库，放在 executeDb 外
    }
    return executeDb(db_, [&](pqxx::work& txn) {
        // 验证清单归属（权限 + 存在性）
        pqxx::result listRes = txn.exec(
            "SELECT name FROM shopping_lists WHERE id = $1 AND user_id = $2",
            pqxx::params{listId, userId});
        if (listRes.empty()) {
            throw ServiceException("购物清单不存在", 404);
        }
        std::string listName = listRes[0]["name"].c_str();

        // 查询清单项明细
        pqxx::result itemsRes = txn.exec(
            "SELECT ingredient_name, to_buy_quantity, unit, checked "
            "FROM shopping_list_items WHERE list_id = $1 ORDER BY id",
            pqxx::params{listId});

        std::string result;
        result += "GoCook 购物清单：" + listName + "\n\n";

        // 构建纯文本内容（手工拼接）
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
        return result;
    }, "数据库操作失败");
}
