#pragma once

#include <gocook/IServices.h>
#include "../DBConnection.h"

class InventoryServiceImpl : public gocook::services::IInventoryService {
public:
    explicit InventoryServiceImpl(DBConnection& db) : db_(db) {}

    // 已实现的核心方法
    gocook::models::PagedInventory getInventory(int userId, int page, int size) override;
    int upsertInventory(int userId, const gocook::models::UpsertInventoryRequest& item) override;
    void deleteInventoryItem(int userId, int itemId) override;

    // 购物清单（多清单模型）
    std::vector<gocook::models::ShoppingListSummary> getShoppingLists(int userId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    int createShoppingList(int userId, const gocook::models::CreateShoppingListRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::ShoppingList getShoppingListDetail(int userId, int listId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void deleteShoppingList(int userId, int listId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void updateShoppingListItem(int userId, int listId, int itemId,
                                const gocook::models::UpdateShoppingItemRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<gocook::models::BatchShoppingItem>& items) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    std::string exportShoppingList(int userId, int listId,
                                   const std::string& format) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

private:
    DBConnection& db_;
};