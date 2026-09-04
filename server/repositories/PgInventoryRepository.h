#pragma once

#include <gocook/IInventoryRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 库存与购物清单仓库的 PostgreSQL 实现，承载 IInventoryRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 InventoryServiceImpl 调用。
 */
class PgInventoryRepository : public gocook::repository::IInventoryRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgInventoryRepository(ConnectionPool& db) : db_(db) {}

    // 查询当前用户库存（分页）
    gocook::models::PagedInventory findInventory(int userId, int page,
                                                 int size) override;
    // 查询当前用户库存（分页 + 食材名模糊过滤；keyword 空串 = 不过滤）
    gocook::models::PagedInventory findInventoryFiltered(
        int userId, int page, int size, const std::string& keyword) override;
    // 添加库存项（同名同单位累加；不同单位新增行），返回库存项 ID
    int upsertInventory(int userId,
                        const gocook::models::UpsertInventoryRequest& item) override;
    // 编辑库存项（按 id 整行替换）
    void updateInventoryItem(int userId, int itemId,
                             const gocook::models::UpsertInventoryRequest& item) override;
    // 删除库存项
    void deleteInventoryItem(int userId, int itemId) override;

    // 查询用户的购物清单列表
    std::vector<gocook::models::ShoppingListSummary> findShoppingLists(
        int userId) override;
    // 创建购物清单（可基于膳食计划自动生成内容），返回清单 ID
    int createShoppingList(
        int userId, const gocook::models::CreateShoppingListRequest& req) override;
    // 查询指定购物清单详情
    gocook::models::ShoppingList findShoppingListDetail(int userId,
                                                        int listId) override;
    // 删除购物清单
    void deleteShoppingList(int userId, int listId) override;
    // 更新购物清单项状态
    void updateShoppingListItem(
        int userId, int listId, int itemId,
        const gocook::models::UpdateShoppingItemRequest& req) override;
    // 批量添加购物清单项，返回处理结果
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<gocook::models::BatchShoppingItem>& items) override;
    // 导出购物清单（仅支持 "text" 纯文本；v2.13 起其他格式如 "image" 抛 400——图片导出为客户端本地能力）
    std::string exportShoppingList(int userId, int listId,
                                   const std::string& format) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
