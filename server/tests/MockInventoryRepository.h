#pragma once

#include <gocook/IInventoryRepository.h>
#include <gmock/gmock.h>

class MockInventoryRepository : public gocook::repository::IInventoryRepository {
public:
    MOCK_METHOD(gocook::models::PagedInventory, findInventory,
                (int, int, int), (override));
    MOCK_METHOD(int, upsertInventory,
                (int, const gocook::models::UpsertInventoryRequest&), (override));
    MOCK_METHOD(void, deleteInventoryItem, (int, int), (override));
    MOCK_METHOD(std::vector<gocook::models::ShoppingListSummary>, findShoppingLists,
                (int), (override));
    MOCK_METHOD(int, createShoppingList,
                (int, const gocook::models::CreateShoppingListRequest&), (override));
    MOCK_METHOD(gocook::models::ShoppingList, findShoppingListDetail,
                (int, int), (override));
    MOCK_METHOD(void, deleteShoppingList, (int, int), (override));
    MOCK_METHOD(void, updateShoppingListItem,
                (int, int, int, const gocook::models::UpdateShoppingItemRequest&), (override));
    MOCK_METHOD(gocook::models::BatchShoppingResponse, batchAddShoppingItems,
                (int, int, const std::vector<gocook::models::BatchShoppingItem>&), (override));
    MOCK_METHOD(std::string, exportShoppingList, (int, int, const std::string&), (override));
};
