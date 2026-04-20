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

    // 以下方法暂时未实现（骨架）
    gocook::models::ShoppingList getShoppingList(int userId,
                                                 std::optional<int> planId = std::nullopt) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    void updateShoppingListItem(int userId, int itemId,
                                const gocook::models::UpdateShoppingItemRequest& request) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, const std::vector<gocook::models::BatchShoppingItem>& items) override {
        throw gocook::services::ServiceException("Not implemented");
    }

private:
    DBConnection& db_;
};