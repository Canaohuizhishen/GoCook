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

// ==================== 库存校验边界 ====================

TEST(InventoryServiceTest, 库存数量为零拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 0.0;
    req.unit = "个";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("必须大于0"));
    }
}

TEST(InventoryServiceTest, 库存数量为负拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = -1.0;
    req.unit = "个";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("必须大于0"));
    }
}

TEST(InventoryServiceTest, 食材单位不合法拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "xyz";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("单位不合法"));
    }
}

// ==================== 未实现的方法 ====================

TEST(InventoryServiceTest, 购物清单列表正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    std::vector<gocook::models::ShoppingListSummary> expected;
    expected.push_back({1, "周末采购", 3, "2026-05-10T14:30:00Z"});
    EXPECT_CALL(*repo, findShoppingLists(1)).WillOnce(Return(expected));

    auto result = service.getShoppingLists(1);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "周末采购");
    EXPECT_EQ(result[0].item_count, 3);
}

TEST(InventoryServiceTest, 创建购物清单正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::CreateShoppingListRequest req;
    req.name = "周末采购";
    EXPECT_CALL(*repo, createShoppingList(1, Truly([](const auto& r) {
        return r.name == "周末采购";
    }))).WillOnce(Return(42));

    int id = service.createShoppingList(1, req);
    EXPECT_EQ(id, 42);
}

TEST(InventoryServiceTest, 购物清单详情正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::ShoppingList expected;
    expected.id = 42;
    expected.name = "周末采购";
    EXPECT_CALL(*repo, findShoppingListDetail(1, 42))
        .WillOnce(Return(expected));

    auto result = service.getShoppingListDetail(1, 42);
    EXPECT_EQ(result.id, 42);
    EXPECT_EQ(result.name, "周末采购");
}

TEST(InventoryServiceTest, 删除清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(repo, deleteShoppingList(1, 42))
        .Times(1);

    service.deleteShoppingList(1, 42);
}

TEST(InventoryServiceTest, 更新清单项正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    UpdateShoppingItemRequest req;
    req.checked = true;

    EXPECT_CALL(repo, updateShoppingListItem(1, 42, 7, _))
        .Times(1);

    service.updateShoppingListItem(1, 42, 7, req);
}

TEST(InventoryServiceTest, 批量添加清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    std::vector<BatchShoppingItem> items;
    items.push_back({"盐", 1.0, "袋"});

    EXPECT_CALL(repo, batchAddShoppingItems(1, 42, _))
        .WillOnce(Return(BatchShoppingResponse{}));

    auto result = service.batchAddShoppingItems(1, 42, items);
    EXPECT_EQ(result.message, "");
}

TEST(InventoryServiceTest, 导出购物清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(repo, exportShoppingList(1, 42, "text"))
        .WillOnce(Return(std::string("GoCook 购物清单：test\n\n[ ] item  1个\n")));

    auto result = service.exportShoppingList(1, 42, "text");
    EXPECT_EQ(result, "GoCook 购物清单：test\n\n[ ] item  1个\n");
}
