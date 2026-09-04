#pragma once

#include <gocook/DataModels.h>
#include <vector>

namespace gocook::repository {

/**
 * @brief 库存与购物清单数据访问抽象接口，定义该域的全部持久化操作。
 *
 * 由 server/repositories/PgInventoryRepository 实现（PostgreSQL），
 * 供 InventoryServiceImpl 依赖注入调用。
 */
class IInventoryRepository {
public:
    virtual ~IInventoryRepository() = default;

    /**
     * @brief 查询当前用户库存（分页）。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的库存列表
     */
    virtual models::PagedInventory findInventory(int userId, int page,
                                                 int size) = 0;

    /**
     * @brief 查询当前用户库存（分页 + 食材名模糊过滤；v2.14 库存页过滤框）。
     *        与 findInventory 语义一致，仅多 keyword 过滤；推荐引擎等既有调用保持走 findInventory。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param keyword 食材名模糊过滤词（空串 = 不过滤；ILIKE 匹配，分页 total 同步过滤）
     * @return 分页的库存列表
     */
    virtual models::PagedInventory findInventoryFiltered(
        int userId, int page, int size, const std::string& keyword) = 0;

    /**
     * @brief 添加库存项（2026-09-04 起语义：同名同单位数量累加；同名不同单位新增行）。
     *        带 expiry_date 的追加视作新批次混入：行内到期日取最早（安全下限）。
     *        实现为单语句原子 UPSERT（ON CONFLICT 累加），并发添加不丢更新。
     * @param userId 用户 ID
     * @param item 库存项数据
     * @return 库存项 ID
     */
    virtual int upsertInventory(int userId,
                                const models::UpsertInventoryRequest& item) = 0;

    /**
     * @brief 编辑库存项（按 id 整行替换，对应 PUT /api/inventory/:id）。
     *        与 upsertInventory 的“累加/新增”语义显式分离。
     *        item 缺省 expiry_date = 清空该列（替换语义）；改名/改单位与本人
     *        另一条 (user_id, ingredient_name, unit) 重复时抛 ServiceException(409)。
     * @param userId 用户 ID
     * @param itemId 库存项 ID（须属于该用户，否则 404）
     * @param item 替换后的库存项数据
     */
    virtual void updateInventoryItem(int userId, int itemId,
                                     const models::UpsertInventoryRequest& item) = 0;

    /**
     * @brief 删除库存项。
     * @param userId 用户 ID
     * @param itemId 库存项 ID
     */
    virtual void deleteInventoryItem(int userId, int itemId) = 0;

    /**
     * @brief 查询用户的购物清单列表。
     * @param userId 用户 ID
     * @return 清单摘要列表
     */
    virtual std::vector<models::ShoppingListSummary> findShoppingLists(
        int userId) = 0;

    /**
     * @brief 创建购物清单。
     * @param userId 用户 ID
     * @param req 清单名与可选的膳食计划 plan_id（提供时自动生成清单内容）
     * @return 新清单 ID
     */
    virtual int createShoppingList(
        int userId, const models::CreateShoppingListRequest& req) = 0;

    /**
     * @brief 查询指定购物清单详情。
     * @param userId 用户 ID
     * @param listId 清单 ID
     * @return 完整清单（含条目）
     */
    virtual models::ShoppingList findShoppingListDetail(int userId,
                                                        int listId) = 0;

    /**
     * @brief 删除购物清单。
     * @param userId 用户 ID
     * @param listId 清单 ID
     */
    virtual void deleteShoppingList(int userId, int listId) = 0;

    /**
     * @brief 更新购物清单项状态（如勾选/取消勾选）。
     * @param userId 用户 ID
     * @param listId 清单 ID
     * @param itemId 清单项 ID
     * @param req 待更新的状态
     */
    virtual void updateShoppingListItem(
        int userId, int listId, int itemId,
        const models::UpdateShoppingItemRequest& req) = 0;

    /**
     * @brief 批量添加购物清单项。
     * @param userId 用户 ID
     * @param listId 清单 ID
     * @param items 待添加的条目列表
     * @return 处理结果（成功消息与完整条目列表）
     */
    virtual models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<models::BatchShoppingItem>& items) = 0;

    /**
     * @brief 导出购物清单内容。
     * @param userId 用户 ID
     * @param listId 清单 ID
     * @param format 导出格式："text" 返回纯文本，"image" 返回 base64 编码图片
     * @return 导出内容
     */
    virtual std::string exportShoppingList(int userId, int listId,
                                           const std::string& format) = 0;
};

} // namespace gocook::repository
