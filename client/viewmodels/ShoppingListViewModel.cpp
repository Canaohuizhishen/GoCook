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
    m_pendingChecks.clear();
    m_pendingItemListIds.clear();
    m_pendingQueue.clear();
    m_inFlightItemId = -1;
    m_inFlightChecked = false;
    m_confirmedChecks.clear();
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
                // 整表替换：重建已确认基线，并把在途/待发乐观值回贴（详情响应可能先于对应 PATCH 生成）
                self->rebuildConfirmedAndReapplyOptimistic();
                emit self->currentListChanged();
                emit self->shoppingListDetailReady();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::updateShoppingListItem(int listId, int itemId, bool checked)
{
    enqueueItemUpdate(listId, itemId, checked);
}

void ShoppingListViewModel::enqueueItemUpdate(int listId, int itemId, bool checked)
{
    // 乐观：点击立即落本地，UI 即时反馈；服务端确认后落定、失败回滚（见 drainItemUpdates 回调）
    applyLocalChecked(itemId, checked);

    // 按条目合并末态：同条目只保留最后一个期望状态；首次出现时入队，保持 FIFO 位置
    if (!m_pendingChecks.contains(itemId)) {
        m_pendingQueue.append(itemId);
        m_pendingItemListIds[itemId] = listId;
    }
    m_pendingChecks[itemId] = checked;

    drainItemUpdates();
}

void ShoppingListViewModel::drainItemUpdates()
{
    if (m_inFlightItemId != -1) return;   // 串行：已有在途请求，等待其回调链继续
    if (m_pendingQueue.isEmpty()) return;

    const int itemId = m_pendingQueue.takeFirst();
    const bool checked = m_pendingChecks.take(itemId);
    const int listId = m_pendingItemListIds.take(itemId);
    m_inFlightItemId = itemId;
    m_inFlightChecked = checked;
    beginLoad();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应整包作废
    // （endLoad 先行配平计数；队列与在途标记已由 clearAll 统一清理）
    const auto session = SessionSnapshot::capture(m_api);

    gocook::models::UpdateShoppingItemRequest req;
    req.checked = checked;

    m_api->updateShoppingListItem(listId, itemId, req,
        [self = QPointer<ShoppingListViewModel>(this), session, itemId, checked]
        (bool success, const std::string& error) {
            if (!self) return;
            self->endLoad();
            if (!session.isCurrent(self->m_api)) return;   // 会话已切换：过期响应作废

            self->m_inFlightItemId = -1;
            self->m_inFlightChecked = false;

            const bool hasNewerPending = self->m_pendingChecks.contains(itemId);
            if (success) {
                // 已确认基线推进到本次发送值；该条目有更新待发时不落本地（防陈旧 ack 闪回）
                self->m_confirmedChecks[itemId] = checked;
                if (!hasNewerPending)
                    self->applyLocalChecked(itemId, checked);
                emit self->itemUpdated();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
                // 无更新待发时回滚到已确认态；未跟踪基线则保持乐观值，等下一次详情加载校正
                if (!hasNewerPending) {
                    auto it = self->m_confirmedChecks.constFind(itemId);
                    if (it != self->m_confirmedChecks.constEnd())
                        self->applyLocalChecked(itemId, it.value());
                }
            }

            // 串行补发下一个待发条目（失败也不卡队列：失败条目已出队，仅回滚显示）
            self->drainItemUpdates();
        });
}

void ShoppingListViewModel::applyLocalChecked(int itemId, bool checked)
{
    // 注：用 value() 只读取键——operator[] 会在键缺失时插入空值，使 clearAll 后的空 map 不再 isEmpty
    QVariantList items = m_currentList.value("items").toList();
    for (int i = 0; i < items.size(); ++i) {
        QVariantMap item = items[i].toMap();
        if (item["id"].toInt() != itemId)
            continue;
        if (item["checked"].toBool() == checked)
            return;   // 无变化：不写不发
        item["checked"] = checked;
        items[i] = item;
        m_currentList["items"] = items;
        emit currentListChanged();
        return;
    }
}

void ShoppingListViewModel::rebuildConfirmedAndReapplyOptimistic()
{
    // 基线：整表替换后的数据即服务端真值（在途条目的值待其 ack 收敛覆盖）
    m_confirmedChecks.clear();
    const QVariantList items = m_currentList.value("items").toList();
    for (const auto& v : items) {
        const QVariantMap item = v.toMap();
        m_confirmedChecks[item["id"].toInt()] = item["checked"].toBool();
    }

    // 在途/待发条目：乐观值回贴（防详情响应先于对应 PATCH 生成造成显示回退）
    if (m_inFlightItemId != -1)
        applyLocalChecked(m_inFlightItemId, m_inFlightChecked);
    for (auto it = m_pendingChecks.constBegin(); it != m_pendingChecks.constEnd(); ++it)
        applyLocalChecked(it.key(), it.value());
}

void ShoppingListViewModel::deleteShoppingListItem(int listId, int itemId)
{
    // 同一条目删除在途时忽略重复点击（QML 无全局禁点，此处兜底去重）
    if (m_deletingItemIds.contains(itemId))
        return;
    // 剔除该条目的待发勾选：防迟到 PATCH 打向已删行（在途勾选不受影响，服务端按序先改后删）
    m_pendingChecks.remove(itemId);
    m_pendingQueue.removeAll(itemId);
    m_pendingItemListIds.remove(itemId);
    m_deletingItemIds.insert(itemId);
    beginLoad();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应作废
    const auto session = SessionSnapshot::capture(m_api);
    m_api->deleteShoppingListItem(listId, itemId,
        [self = QPointer<ShoppingListViewModel>(this), session, itemId](bool success, const std::string& error) {
            if (!self) return;
            self->endLoad();
            if (!session.isCurrent(self->m_api)) return;   // 会话已切换：过期响应作废

            self->m_deletingItemIds.remove(itemId);
            self->m_confirmedChecks.remove(itemId);

            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                // 条目保留可见：若其勾选未在途，回滚显示到已确认态（可能已剔除未发送的乐观值）
                if (self->m_inFlightItemId != itemId) {
                    auto it = self->m_confirmedChecks.constFind(itemId);
                    if (it != self->m_confirmedChecks.constEnd())
                        self->applyLocalChecked(itemId, it.value());
                }
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
                self->rebuildConfirmedAndReapplyOptimistic();
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
            self->rebuildConfirmedAndReapplyOptimistic();
            emit self->currentListChanged();
            emit self->batchAddComplete(QStringLiteral("已成功添加 %1 项").arg(static_cast<int>(data.items.size())));
            emit self->shoppingListCreated(name);
            self->refresh();
        });
}
