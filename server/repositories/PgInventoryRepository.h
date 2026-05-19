#pragma once

#include <gocook/IInventoryRepository.h>
#include "../common/ConnectionPool.h"

class PgInventoryRepository : public gocook::repository::IInventoryRepository {
public:
    explicit PgInventoryRepository(ConnectionPool& db) : db_(db) {}

    gocook::models::PagedInventory findInventory(int userId, int page,
                                                 int size) override;
    int upsertInventory(int userId,
                        const gocook::models::UpsertInventoryRequest& item) override;
    void deleteInventoryItem(int userId, int itemId) override;

    std::vector<gocook::models::ShoppingListSummary> findShoppingLists(
        int userId) override;
    int createShoppingList(
        int userId, const gocook::models::CreateShoppingListRequest& req) override;
    gocook::models::ShoppingList findShoppingListDetail(int userId,
                                                        int listId) override;
    void deleteShoppingList(int userId, int listId) override;
    void updateShoppingListItem(
        int userId, int listId, int itemId,
        const gocook::models::UpdateShoppingItemRequest& req) override;
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<gocook::models::BatchShoppingItem>& items) override;
    std::string exportShoppingList(int userId, int listId,
                                   const std::string& format) override;

private:
    ConnectionPool& db_;
};
