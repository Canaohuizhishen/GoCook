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
        for (const auto& summary : data)
            self->m_shoppingLists.append(DataMapper::toMap(summary));

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
