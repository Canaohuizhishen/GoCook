#include "InventoryServiceImpl.h"
#include <unordered_set>

using namespace gocook::models;
using namespace gocook::services;

namespace {
    const std::unordered_set<std::string>& validUnits() {
        static const std::unordered_set<std::string> units = {
            "个", "克", "千克", "毫升", "升", "只", "条", "把", "根",
            "片", "块", "袋", "包", "盒", "瓶", "碗", "勺",
            "茶匙", "汤匙", "斤", "两", "磅", "份"
        };
        return units;
    }
}

PagedInventory InventoryServiceImpl::getInventory(int userId, int page, int size) {
    return inventoryRepo_->findInventory(userId, page, size);
}

int InventoryServiceImpl::upsertInventory(int userId, const UpsertInventoryRequest& item) {
    // 库存数量校验
    if (item.quantity <= 0.0) {
        throw ServiceException("库存数量必须大于0", 400);
    }
    // 食材单位校验
    if (!validUnits().count(item.unit)) {
        throw ServiceException("食材单位不合法", 400);
    }
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
void InventoryServiceImpl::deleteShoppingList(int userId, int listId) {
    inventoryRepo_->deleteShoppingList(userId, listId);
}
void InventoryServiceImpl::updateShoppingListItem(int userId, int listId, int itemId,
                                                       const UpdateShoppingItemRequest& request) {
    inventoryRepo_->updateShoppingListItem(userId, listId, itemId, request);
}
BatchShoppingResponse InventoryServiceImpl::batchAddShoppingItems(int userId, int listId, const std::vector<BatchShoppingItem>& items) {
    return inventoryRepo_->batchAddShoppingItems(userId, listId, items);
}
std::string InventoryServiceImpl::exportShoppingList(int userId, int listId, const std::string& format) {
    return inventoryRepo_->exportShoppingList(userId, listId, format);
}
