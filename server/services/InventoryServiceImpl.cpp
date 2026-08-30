#include "InventoryServiceImpl.h"
#include <unordered_set>
#include <regex>

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

    // 真实日历校验：月/日必须构成有效日期（含闰年 2 月 29 天）。
    // 仅做月/日范围检查会放行 2026-02-31 / 2026-04-31 这类不存在日期，打穿 DB 层变 500。
    bool isValidCalendarDate(int year, int month, int day) {
        if (month < 1 || month > 12 || day < 1) return false;
        static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int dim = daysInMonth[month - 1];
        if (month == 2) {
            const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            if (leap) dim = 29;
        }
        return day <= dim;
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
    // 过期日期校验：非 YYYY-MM-DD 或不存在的日期（如 2026-02-31）打穿 DB 层会变成 500，这里提前转 400 可读错误
    if (item.expiry_date.has_value()) {
        const std::string& d = item.expiry_date.value();
        static const std::regex dateRe(R"(^(\d{4})-(\d{1,2})-(\d{1,2})$)");
        std::smatch m;
        if (!std::regex_match(d, m, dateRe)) {
            throw ServiceException("过期日期格式无效，应为 YYYY-MM-DD", 400);
        }
        const int year = std::stoi(m[1].str());
        const int month = std::stoi(m[2].str());
        const int day = std::stoi(m[3].str());
        if (!isValidCalendarDate(year, month, day)) {
            throw ServiceException("过期日期无效", 400);
        }
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
