#pragma once

#include <gocook/DataModels.h>
#include <vector>

namespace gocook::repository {

class IInventoryRepository {
public:
    virtual ~IInventoryRepository() = default;

    virtual models::PagedInventory findInventory(int userId, int page,
                                                 int size) = 0;
    virtual int upsertInventory(int userId,
                                const models::UpsertInventoryRequest& item) = 0;
    virtual void deleteInventoryItem(int userId, int itemId) = 0;

    virtual std::vector<models::ShoppingListSummary> findShoppingLists(
        int userId) = 0;
    virtual int createShoppingList(
        int userId, const models::CreateShoppingListRequest& req) = 0;
    virtual models::ShoppingList findShoppingListDetail(int userId,
                                                        int listId) = 0;
    virtual void deleteShoppingList(int userId, int listId) = 0;
    virtual void updateShoppingListItem(
        int userId, int listId, int itemId,
        const models::UpdateShoppingItemRequest& req) = 0;
    virtual models::BatchShoppingResponse batchAddShoppingItems(
        int userId, int listId,
        const std::vector<models::BatchShoppingItem>& items) = 0;
    virtual std::string exportShoppingList(int userId, int listId,
                                           const std::string& format) = 0;
};

} // namespace gocook::repository
