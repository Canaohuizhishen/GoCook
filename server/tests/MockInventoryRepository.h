#pragma once

#include <gocook/IInventoryRepository.h>
#include <gmock/gmock.h>

/**
 * @brief IInventoryRepository 的 Google Mock 替身，用于服务层单元测试。
 *
 * 在测试中通过 EXPECT_CALL 为每个方法设置预期调用与返回值，
 * 方法契约见 IInventoryRepository.h 接口文档。
 */
class MockInventoryRepository : public gocook::repository::IInventoryRepository {
public:
    // 查询当前用户库存
    MOCK_METHOD(gocook::models::PagedInventory, findInventory,
                (int, int, int), (override));
    // 添加库存项（同名同单位累加；不同单位新增行），返回库存项 ID
    MOCK_METHOD(int, upsertInventory,
                (int, const gocook::models::UpsertInventoryRequest&), (override));
    // 编辑库存项（按 id 整行替换）
    MOCK_METHOD(void, updateInventoryItem,
                (int, int, const gocook::models::UpsertInventoryRequest&), (override));
    // 删除库存项
    MOCK_METHOD(void, deleteInventoryItem, (int, int), (override));
    // 查询购物清单列表
    MOCK_METHOD(std::vector<gocook::models::ShoppingListSummary>, findShoppingLists,
                (int), (override));
    // 创建购物清单，返回清单 ID
    MOCK_METHOD(int, createShoppingList,
                (int, const gocook::models::CreateShoppingListRequest&), (override));
    // 查询购物清单详情
    MOCK_METHOD(gocook::models::ShoppingList, findShoppingListDetail,
                (int, int), (override));
    // 删除购物清单
    MOCK_METHOD(void, deleteShoppingList, (int, int), (override));
    // 更新购物清单项状态
    MOCK_METHOD(void, updateShoppingListItem,
                (int, int, int, const gocook::models::UpdateShoppingItemRequest&), (override));
    // 批量添加购物清单项
    MOCK_METHOD(gocook::models::BatchShoppingResponse, batchAddShoppingItems,
                (int, int, const std::vector<gocook::models::BatchShoppingItem>&), (override));
    // 导出购物清单
    MOCK_METHOD(std::string, exportShoppingList, (int, int, const std::string&), (override));
};
