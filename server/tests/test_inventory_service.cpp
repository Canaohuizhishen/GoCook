#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../services/InventoryServiceImpl.h"
#include "MockInventoryRepository.h"

using namespace testing;
using namespace gocook::models;
using namespace gocook::services;
using namespace gocook::repository;

namespace {
    PagedInventory makePagedInventory(int count = 2) {
        PagedInventory result;
        for (int i = 1; i <= count; ++i)
            result.data.push_back({i, "Ingredient " + std::to_string(i),
                                   1.0 * i, "kg", std::nullopt, "2026-01-0" + std::to_string(i)});
        result.pagination = {1, count, 5, 1};
        return result;
    }

    UpsertInventoryRequest makeUpsertReq() {
        return {"Tomato", 3.0, "个", std::nullopt};
    }
}

// ==================== 已实现的方法 ====================

TEST(InventoryServiceTest, 获取库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    auto expected = makePagedInventory(2);
    EXPECT_CALL(*repo, findInventory(1, 1, 20)).WillOnce(Return(expected));

    auto result = service.getInventory(1, 1, 20);
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.data[0].ingredient_name, "Ingredient 1");
    EXPECT_EQ(result.data[1].ingredient_name, "Ingredient 2");
}

TEST(InventoryServiceTest, 库存分页参数透传) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findInventory(2, 3, 10))
        .WillOnce(Return(PagedInventory{}));

    auto result = service.getInventory(2, 3, 10);
    EXPECT_TRUE(result.data.empty());
}

TEST(InventoryServiceTest, 更新库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    auto req = makeUpsertReq();
    EXPECT_CALL(*repo, upsertInventory(1, Truly([](const auto& r) {
        return r.ingredient_name == "Tomato" && r.quantity == 3.0;
    }))).WillOnce(Return(42));

    int id = service.upsertInventory(1, req);
    EXPECT_EQ(id, 42);
}

TEST(InventoryServiceTest, 删除库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, deleteInventoryItem(1, 5)).Times(1);
    EXPECT_NO_THROW(service.deleteInventoryItem(1, 5));
}

// ==================== 未实现的方法 ====================

TEST(InventoryServiceTest, 购物清单列表未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getShoppingLists(1), ServiceException);
}

TEST(InventoryServiceTest, 创建购物清单未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.createShoppingList(1, {}), ServiceException);
}

TEST(InventoryServiceTest, 购物清单详情未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getShoppingListDetail(1, 1), ServiceException);
}

TEST(InventoryServiceTest, 删除购物清单未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.deleteShoppingList(1, 1), ServiceException);
}

TEST(InventoryServiceTest, 更新清单项未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.updateShoppingListItem(1, 1, 1, {}), ServiceException);
}

TEST(InventoryServiceTest, 批量添加清单未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.batchAddShoppingItems(1, 1, {}), ServiceException);
}

TEST(InventoryServiceTest, 导出购物清单未实现) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_THROW(service.exportShoppingList(1, 1, "text"), ServiceException);
}
