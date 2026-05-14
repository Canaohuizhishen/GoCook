#pragma once

#include <gocook/IServices.h>
#include <gocook/IInventoryRepository.h>
#include <memory>

class InventoryServiceImpl : public gocook::services::IInventoryService {
public:
    explicit InventoryServiceImpl(std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo)
        : inventoryRepo_(std::move(inventoryRepo)) {}

    gocook::models::PagedInventory getInventory(int userId, int page, int size) override;
    int upsertInventory(int userId, const gocook::models::UpsertInventoryRequest& item) override;
    void deleteInventoryItem(int userId, int itemId) override;

    std::vector<gocook::models::ShoppingListSummary> getShoppingLists(int userId) override;
    int createShoppingList(int userId, const gocook::models::CreateShoppingListRequest& request) override;
    gocook::models::ShoppingList getShoppingListDetail(int userId, int listId) override;
    void deleteShoppingList(int userId, int listId) override;
    void updateShoppingListItem(int userId, int listId, int itemId,
                                const gocook::models::UpdateShoppingItemRequest& request) override;
    gocook::models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<gocook::models::BatchShoppingItem>& items) override;
    std::string exportShoppingList(int userId, int listId,
                                   const std::string& format) override;

private:
    std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo_;
};
