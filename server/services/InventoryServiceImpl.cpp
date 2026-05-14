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
std::vector<ShoppingListSummary> InventoryServiceImpl::getShoppingLists(int) {
    throw ServiceException("Not implemented", 501);
}
int InventoryServiceImpl::createShoppingList(int, const CreateShoppingListRequest&) {
    throw ServiceException("Not implemented", 501);
}
ShoppingList InventoryServiceImpl::getShoppingListDetail(int, int) {
    throw ServiceException("Not implemented", 501);
}
void InventoryServiceImpl::deleteShoppingList(int, int) {
    throw ServiceException("Not implemented", 501);
}
void InventoryServiceImpl::updateShoppingListItem(int, int, int, const UpdateShoppingItemRequest&) {
    throw ServiceException("Not implemented", 501);
}
BatchShoppingResponse InventoryServiceImpl::batchAddShoppingItems(int, int, const std::vector<BatchShoppingItem>&) {
    throw ServiceException("Not implemented", 501);
}
std::string InventoryServiceImpl::exportShoppingList(int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
