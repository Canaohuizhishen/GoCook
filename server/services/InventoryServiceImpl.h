#pragma once

#include <gocook/IServices.h>
#include <gocook/IInventoryRepository.h>
#include <memory>
#include <string>
#include <unordered_set>

/**
 * @brief 库存与购物清单服务实现，承载 IInventoryService 接口定义的全部业务逻辑。
 *
 * 依赖库存仓库抽象完成数据访问，由 InventoryHandler 调用。
 */
class InventoryServiceImpl : public gocook::services::IInventoryService {
public:
    // 库存食材单位白名单（唯一事实源）：客户端 InventoryPage.qml 的 supportedUnits 为提交前拦截副本，
    // 两侧须保持一致（server/tests/test_inventory_service.cpp 的同步守卫用例强制校验，改动须两侧同步）；
    // 空/纯空白提交默认 "克"（客户端规范化），服务端按精确匹配校验
    static const std::unordered_set<std::string>& validUnits();

    // 构造函数，注入库存仓库抽象
    explicit InventoryServiceImpl(std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo)
        : inventoryRepo_(std::move(inventoryRepo)) {}

    // 获取当前用户库存（分页；keyword 非空时按食材名模糊过滤；默认空词 = 不过滤）
    gocook::models::PagedInventory getInventory(int userId, int page, int size,
                                                const std::string& keyword = "") override;
    // 添加库存项（同名同单位累加；不同单位新增行），返回库存项 ID
    int upsertInventory(int userId, const gocook::models::UpsertInventoryRequest& item) override;
    // 编辑库存项（按 id 整行替换）
    void updateInventoryItem(int userId, int itemId,
                             const gocook::models::UpsertInventoryRequest& item) override;
    // 删除库存项
    void deleteInventoryItem(int userId, int itemId) override;

    // 获取用户的购物清单列表
    std::vector<gocook::models::ShoppingListSummary> getShoppingLists(int userId) override;
    // 创建购物清单（可基于膳食计划 plan_id 自动生成内容），返回清单 ID
    int createShoppingList(int userId, const gocook::models::CreateShoppingListRequest& request) override;
    // 获取指定购物清单详情
    gocook::models::ShoppingList getShoppingListDetail(int userId, int listId) override;
    // 删除购物清单
    void deleteShoppingList(int userId, int listId) override;
    // 更新购物清单项状态（如勾选/取消勾选）
    void updateShoppingListItem(int userId, int listId, int itemId,
                                const gocook::models::UpdateShoppingItemRequest& request) override;
    // 批量添加购物清单项，返回处理结果
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<gocook::models::BatchShoppingItem>& items) override;
    // 导出购物清单（仅支持 "text" 纯文本；v2.13 起其他格式如 "image" 抛 400——图片导出为客户端本地能力）
    std::string exportShoppingList(int userId, int listId,
                                   const std::string& format) override;

private:
    std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo_; ///< 库存仓库抽象
};
