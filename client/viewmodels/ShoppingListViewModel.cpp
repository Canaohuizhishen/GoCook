#include "ShoppingListViewModel.h"
#include <DataMapper.h>
#include <QPointer>

ShoppingListViewModel::ShoppingListViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList ShoppingListViewModel::shoppingLists() const { return m_shoppingLists; }
QVariantMap ShoppingListViewModel::currentList() const { return m_currentList; }
void ShoppingListViewModel::beginLoad()
{
    bool wasIdle = (m_pendingRequests == 0);
    m_pendingRequests++;
    if (wasIdle) {
        emit isLoadingChanged();
    }
}

void ShoppingListViewModel::endLoad()
{
    m_pendingRequests--;
    if (m_pendingRequests == 0) {
        emit isLoadingChanged();
    }
}

bool ShoppingListViewModel::creating() const { return m_creating; }

void ShoppingListViewModel::refresh()
{
    loadShoppingLists();
}

void ShoppingListViewModel::exportShoppingList(int listId)
{
    beginLoad();

    m_api->exportShoppingList(listId, "text",
        [self = QPointer<ShoppingListViewModel>(this)](bool success, const std::string& content, const std::string& error) {
            if (!self) return;
            self->endLoad();

            if (success) {
                emit self->exportReady(QString::fromStdString(content));
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::loadShoppingLists()
{
    beginLoad();

    m_api->getShoppingLists([self = QPointer<ShoppingListViewModel>(this)](bool success,
                                const std::vector<gocook::models::ShoppingListSummary>& data,
                                const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->errorOccurred(QString::fromStdString(error));
            self->endLoad();
            return;
        }

        self->m_shoppingLists.clear();
        for (const auto& summary : data) {
            // 跳过正在删除中的条目（DELETE 尚未到达服务端，但乐观删除已从 UI 移除）
            if (self->m_pendingDeleteIds.contains(summary.id))
                continue;
            self->m_shoppingLists.append(DataMapper::toMap(summary));
        }

        emit self->shoppingListsChanged();
        self->endLoad();
    });
}

void ShoppingListViewModel::loadShoppingListDetail(int listId)
{
    beginLoad();

    m_api->getShoppingListDetail(listId,
        [self = QPointer<ShoppingListViewModel>(this)](bool success,
                              const gocook::models::ShoppingList& data,
                              const std::string& error) {
            if (!self) return;
            self->endLoad();

            if (success) {
                self->m_currentList = DataMapper::toMap(data);
                emit self->currentListChanged();
                emit self->shoppingListDetailReady();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::updateShoppingListItem(int listId, int itemId, bool checked)
{
    // 快速连续点击：只记最后一次状态，等当前请求返回后自动发送
    if (m_updateInFlight) {
        m_updatePendingItemId = itemId;
        m_updatePendingChecked = checked;
        return;
    }

    m_updateInFlight = true;
    beginLoad();

    gocook::models::UpdateShoppingItemRequest req;
    req.checked = checked;

    m_api->updateShoppingListItem(listId, itemId, req,
        [self = QPointer<ShoppingListViewModel>(this), listId, itemId, checked](bool success, const std::string& error) {
            if (!self) return;
            self->m_updateInFlight = false;

            if (success) {
                // 局部更新 checked 状态，避免二次 HTTP 请求刷新全表（刷表可能间歇性失败）
                QVariantList items = self->m_currentList["items"].toList();
                for (int i = 0; i < items.size(); ++i) {
                    QVariantMap item = items[i].toMap();
                    if (item["id"].toInt() == itemId) {
                        item["checked"] = checked;
                        items[i] = item;
                        break;
                    }
                }
                self->m_currentList["items"] = items;
                self->endLoad();
                emit self->currentListChanged();
                emit self->itemUpdated();
            } else {
                self->endLoad();
                emit self->errorOccurred(QString::fromStdString(error));
            }

            // 如果有排队的点击，立即发送
            if (self->m_updatePendingItemId != -1) {
                int pendingId = self->m_updatePendingItemId;
                bool pendingChecked = self->m_updatePendingChecked;
                self->m_updatePendingItemId = -1;
                self->updateShoppingListItem(listId, pendingId, pendingChecked);
            }
        });
}

void ShoppingListViewModel::deleteShoppingList(int listId)
{
    m_pendingDeleteIds.insert(listId);
    m_deletingListId = listId;
    emit deletingListIdChanged();
    beginLoad();

    m_api->deleteShoppingList(listId,
        [self = QPointer<ShoppingListViewModel>(this), listId](bool success, const std::string& error) {
            if (!self) return;
            self->m_pendingDeleteIds.remove(listId);
            self->m_deletingListId = -1;
            emit self->deletingListIdChanged();
            self->endLoad();

            if (success) {
                // 移除本地缓存中的该清单
                QVariantList lists = self->m_shoppingLists;
                for (int i = 0; i < lists.size(); ++i) {
                    if (lists[i].toMap()["id"].toInt() == listId) {
                        lists.removeAt(i);
                        break;
                    }
                }
                self->m_shoppingLists = lists;
                emit self->shoppingListsChanged();
                emit self->shoppingListDeleted(listId);
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::deleteShoppingListOptimistic(int listId, QVariantMap listData)
{
    // 乐观删除：立即从列表移除，API 失败时插回
    m_pendingDeleteIds.insert(listId);

    int removedIndex = -1;
    QVariantList lists = m_shoppingLists;
    for (int i = 0; i < lists.size(); ++i) {
        if (lists[i].toMap()["id"].toInt() == listId) {
            removedIndex = i;
            lists.removeAt(i);
            break;
        }
    }
    m_shoppingLists = lists;
    emit shoppingListsChanged();

    m_api->deleteShoppingList(listId,
        [self = QPointer<ShoppingListViewModel>(this), listId, listData, removedIndex](bool success, const std::string& error) {
            if (!self) return;

            self->m_pendingDeleteIds.remove(listId);

            if (success) {
                emit self->shoppingListDeleted(listId);
            } else {
                // 失败：插回原位置
                QVariantList lists = self->m_shoppingLists;
                if (removedIndex >= 0 && removedIndex <= lists.size()) {
                    lists.insert(removedIndex, listData);
                } else {
                    lists.prepend(listData);
                }
                self->m_shoppingLists = lists;
                emit self->shoppingListsChanged();
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::batchAddShoppingItems(int listId, const QVariantList& items)
{
    beginLoad();

    std::vector<gocook::models::BatchShoppingItem> batchItems;
    for (const auto& val : items) {
        QVariantMap map = val.toMap();
        gocook::models::BatchShoppingItem item;
        item.ingredient_name = map["ingredient_name"].toString().toStdString();
        item.quantity = map["quantity"].toDouble();
        item.unit = map["unit"].toString().toStdString();
        batchItems.push_back(std::move(item));
    }

    m_api->batchAddShoppingItems(listId, batchItems,
        [self = QPointer<ShoppingListViewModel>(this), listId](bool success,
                              const gocook::models::BatchShoppingResponse& resp,
                              const std::string& error) {
            if (!self) return;
            self->endLoad();

            if (success) {
                // 刷新详情以反映最新数据
                self->loadShoppingListDetail(listId);
                emit self->batchAddComplete(QString::fromStdString(resp.message));
            } else {
                emit self->batchAddFailed(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::createShoppingList(const QString& name)
{
    m_creating = true;
    emit creatingChanged();

    gocook::models::CreateShoppingListRequest req;
    req.name = name.toStdString();

    m_api->createShoppingList(req,
        [self = QPointer<ShoppingListViewModel>(this), name](bool success,
                              const gocook::models::ShoppingList& data,
                              const std::string& error) {
            if (!self) return;
            self->m_creating = false;
            emit self->creatingChanged();

            if (success) {
                self->m_currentList = DataMapper::toMap(data);
                emit self->currentListChanged();
                emit self->shoppingListCreated(name);
                self->refresh();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
                emit self->shoppingListCreateFailed(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::createListFromRecipe(const QString& name,
                                                  const QVariantList& missingIngredients)
{
    beginLoad();
    m_creating = true;
    emit creatingChanged();

    gocook::models::CreateShoppingListRequest req;
    req.name = name.toStdString();

    m_api->createShoppingList(req,
        [self = QPointer<ShoppingListViewModel>(this), name, missingIngredients]
        (bool success, const gocook::models::ShoppingList& data, const std::string& error) {
            if (!self) return;
            self->m_creating = false;
            emit self->creatingChanged();

            if (!success) {
                self->endLoad();
                emit self->shoppingListCreateFailed(QString::fromStdString(error));
                return;
            }

            int listId = data.id;

            std::vector<gocook::models::BatchShoppingItem> batchItems;
            for (const auto& val : missingIngredients) {
                QVariantMap map = val.toMap();
                gocook::models::BatchShoppingItem item;
                item.ingredient_name = map["name"].toString().toStdString();
                item.quantity = map["quantity"].toDouble();
                item.unit = map["unit"].toString().toStdString();
                batchItems.push_back(std::move(item));
            }

            if (batchItems.empty()) {
                self->endLoad();
                self->m_currentList = DataMapper::toMap(data);
                emit self->currentListChanged();
                emit self->shoppingListCreated(name);
                self->refresh();
                return;
            }

            self->m_api->batchAddShoppingItems(listId, batchItems,
                [self = QPointer<ShoppingListViewModel>(self), listId, name, data]
                (bool ok, const gocook::models::BatchShoppingResponse& resp,
                 const std::string& batchError) {
                    if (!self) return;
                    self->endLoad();
                    if (ok) {
                        self->loadShoppingListDetail(listId);
                        emit self->batchAddComplete(QString::fromStdString(resp.message));
                        // 成功后标记列表已创建
                        self->m_currentList = DataMapper::toMap(data);
                        emit self->currentListChanged();
                        emit self->shoppingListCreated(name);
                    } else {
                        emit self->batchAddFailed(QString::fromStdString(batchError));
                    }
                    self->refresh();
                });
        });
}
