#include "InventoryServiceImpl.h"

using namespace gocook::models;
using namespace gocook::services;

PagedInventory InventoryServiceImpl::getInventory(int userId, int page, int size) {
    return inventoryRepo_->findInventory(userId, page, size);
}

int InventoryServiceImpl::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    return inventoryRepo_->upsertInventory(userId, item);
}

void InventoryServiceImpl::deleteInventoryItem(int userId, int itemId) {
    inventoryRepo_->deleteInventoryItem(userId, itemId);
}

// 购物清单方法暂时未实现
std::vector<ShoppingListSummary> InventoryServiceImpl::getShoppingLists(int userId) {
    return inventoryRepo_->findShoppingLists(userId);
}
int InventoryServiceImpl::createShoppingList(int userId, const CreateShoppingListRequest& request) {
    return inventoryRepo_->createShoppingList(userId, request);
}
ShoppingList InventoryServiceImpl::getShoppingListDetail(int userId, int listId) {
    return inventoryRepo_->findShoppingListDetail(userId, listId);
}
void InventoryServiceImpl::deleteShoppingList(int, int) {
    throw ServiceException("Not implemented", 501);
}
void InventoryServiceImpl::updateShoppingListItem(int, int, int, const UpdateShoppingItemRequest&) {
    throw ServiceException("Not implemented", 501);
}
BatchShoppingResponse InventoryServiceImpl::batchAddShoppingItems(int userId, int listId, const std::vector<BatchShoppingItem>& items) {
    return inventoryRepo_->batchAddShoppingItems(userId, listId, items);
}
std::string InventoryServiceImpl::exportShoppingList(int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
