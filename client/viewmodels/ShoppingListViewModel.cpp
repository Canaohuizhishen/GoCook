#include "ShoppingListViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include "RequestGuards.h"

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

void ShoppingListViewModel::clearAll()
{
    // 登出统一清理（main.cpp 单点接线）：列表 / 详情 / 乐观删除集合 / 勾选合并与建单标记全部归零。
    // 刻意不动 m_pendingRequests：在途请求的回调仍会调用 endLoad 配对计数，清掉会负漂移
    m_shoppingLists.clear();
    m_currentList = {};
    m_pendingDeleteIds.clear();
    m_deletingItemIds.clear();
    m_updatePendingItemId = -1;
    m_updatePendingChecked = false;
    m_updateInFlight = false;
    m_creating = false;
    emit shoppingListsChanged();
    emit currentListChanged();
    emit creatingChanged();
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

    // 快照当前会话（token，SessionSnapshot）：响应到达时若会话已切换（登出/换号），
    // 该在途响应属于旧账号——静默丢弃，避免旧账号清单串入当前界面
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getShoppingLists([self = QPointer<ShoppingListViewModel>(this), session](bool success,
                                const std::vector<gocook::models::ShoppingListSummary>& data,
                                const std::string& error) {
        if (!self) return;
        self->endLoad();   // 先配对计数：过期响应也不能让 isLoading 卡死
        if (!session.isCurrent(self->m_api)) return;   // 会话已切换：过期响应作废

        if (!success) {
            emit self->errorOccurred(QString::fromStdString(error));
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
    });
}

void ShoppingListViewModel::loadShoppingListDetail(int listId)
{
    beginLoad();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期详情静默丢弃
    // （endLoad 先行配平计数）
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getShoppingListDetail(listId,
        [self = QPointer<ShoppingListViewModel>(this), session](bool success,
                              const gocook::models::ShoppingList& data,
                              const std::string& error) {
            if (!self) return;
            self->endLoad();
            if (!session.isCurrent(self->m_api)) return;   // 会话已切换：过期响应作废

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

void ShoppingListViewModel::deleteShoppingListItem(int listId, int itemId)
{
    // 同一条目删除在途时忽略重复点击（QML 侧同时有 isLoading 禁用，此处兜底）
    if (m_deletingItemIds.contains(itemId))
        return;
    m_deletingItemIds.insert(itemId);
    beginLoad();

    m_api->deleteShoppingListItem(listId, itemId,
        [self = QPointer<ShoppingListViewModel>(this), itemId](bool success, const std::string& error) {
            if (!self) return;
            self->m_deletingItemIds.remove(itemId);
            self->endLoad();

            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                return;
            }

            // 局部移除该条目，避免整表重拉（与勾选局部更新同思路）
            QVariantList items = self->m_currentList["items"].toList();
            for (int i = 0; i < items.size(); ++i) {
                if (items[i].toMap()["id"].toInt() == itemId) {
                    items.removeAt(i);
                    break;
                }
            }
            self->m_currentList["items"] = items;
            emit self->currentListChanged();
            emit self->itemUpdated();
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
    // v2.18 复合建单：条目随建单请求一次送达，服务端单事务落库（失败零残留，不再有"空清单"中间态）
    for (const auto& val : missingIngredients) {
        QVariantMap map = val.toMap();
        gocook::models::BatchShoppingItem item;
        item.ingredient_name = map["name"].toString().toStdString();
        item.quantity = map["quantity"].toDouble();
        item.unit = map["unit"].toString().toStdString();
        req.items.push_back(std::move(item));
    }

    m_api->createShoppingList(req,
        [self = QPointer<ShoppingListViewModel>(this), name]
        (bool success, const gocook::models::ShoppingList& data, const std::string& error) {
            if (!self) return;
            self->m_creating = false;
            emit self->creatingChanged();
            self->endLoad();

            if (!success) {
                // 复合端点失败 = 服务端整体回滚（未创建任何东西）：推荐卡片走批量失败反馈
                emit self->shoppingListCreateFailed(QString::fromStdString(error));
                emit self->batchAddFailed(QString::fromStdString(error));
                return;
            }

            // 创建响应已携带完整条目（含 to_buy 计算），无需再拉一次详情
            self->m_currentList = DataMapper::toMap(data);
            emit self->currentListChanged();
            emit self->batchAddComplete(QStringLiteral("已成功添加 %1 项").arg(static_cast<int>(data.items.size())));
            emit self->shoppingListCreated(name);
            self->refresh();
        });
}
