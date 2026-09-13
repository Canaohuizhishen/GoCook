#include "InventoryViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include "../api/HttpGoCookApi.h"
#include "../database/LocalDatabase.h"

namespace {
// 快照兜底的本地过滤：近似服务端 ingredient_name ILIKE '%kw%'（子串匹配、忽略大小写）语义。
// 仅用于断网兜底展示（服务端不参与），恢复联网/重试成功后由服务端结果替换。
// 快照条目为 DataMapper camelCase map，名称键为 "ingredientName"；空词返回全量。
QVariantList filterSnapshotByKeyword(const QVariantList& items, const QString& keyword)
{
    if (keyword.isEmpty())
        return items;
    QVariantList filtered;
    for (const QVariant& item : items) {
        if (item.toMap().value("ingredientName").toString().contains(keyword, Qt::CaseInsensitive))
            filtered.append(item);
    }
    return filtered;
}
} // namespace

InventoryViewModel::InventoryViewModel(IGoCookApi *api, QObject *parent, LocalDatabase *db)
    : QObject(parent)
    , m_api(api)
    , m_db(db ? db : LocalDatabase::instance())
{
    // 退避重试：单次触发，tick 里重拉首页；成功/clearAll/登出时停表（见各分支）
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &InventoryViewModel::onRetryTick);
}

QVariantList InventoryViewModel::items() const { return m_items; }
bool InventoryViewModel::isLoading() const { return m_isLoading; }
bool InventoryViewModel::hasMore() const { return m_hasMore; }
QString InventoryViewModel::filterText() const { return m_filterText; }
bool InventoryViewModel::loadFailed() const { return m_loadFailed; }
QString InventoryViewModel::loadFailedMessage() const { return m_loadFailedMessage; }

void InventoryViewModel::setFilterText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_filterText) return;
    m_filterText = trimmed;
    emit filterTextChanged();
    // 回到第一页按新词加载。若上一请求仍在途，loadInventory 会递增代次并立即发出新请求
    // （不再等旧请求回归）：旧响应到达时代次落后被静默丢弃，最后一次输入必达且不押后
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::refresh()
{
    m_currentPage = 1;
    m_items.clear();
    emit itemsChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::clearAll()
{
    m_items.clear();
    m_hasMore = false;
    m_currentPage = 1;
    m_totalPages = 0;
    m_isLoading = false;
    ++m_epoch;  // 作废在途响应：旧账号/旧轮次的回调到达时代次落后，静默退出
    m_filterText.clear();
    m_lastLoadFailed = false;
    m_itemsFromSnapshotFallback = false;
    stopRetry();
    clearLoadFailed();
    emit filterTextChanged();
    emit itemsChanged();
    emit hasMoreChanged();
    emit isLoadingChanged();
}

void InventoryViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadInventory(m_currentPage + 1, m_pageSize);
}

void InventoryViewModel::loadInventory(int page, int size)
{
    // 翻页（page>1）是当前轮次的延续：仅当无在途请求时才发出，防止翻页请求堆叠。
    // 首屏/换词/刷新（page<=1）= 开启新一轮：递增代次并立即发送，不被在途请求阻塞——
    // 旧响应到达时代次已落后，由回调侧静默丢弃（见下），无需此处等待或事后补发
    // 注：page>1 的唯一入口是 loadNextPage()（其已前置 m_isLoading 拦截），本守卫为防御性
    if (page > 1 && m_isLoading) return;
    if (page <= 1) ++m_epoch;
    m_isLoading = true;
    emit isLoadingChanged();

    // 快照当前会话（token）与请求代次：响应到达时若会话已切换（登出/换号），该在途响应属于旧账号——
    // 静默丢弃，避免旧账号数据串入当前界面；会话未变但代次已落后（在途期间换词/刷新/clearAll），
    // 本响应已被更新的请求取代——同样静默丢弃（数据由最新请求填充，避免旧词结果闪回）
    const std::string tokenAtSend = m_api->authToken();
    const int epochAtSend = m_epoch;
    m_api->getInventory(page, size, m_filterText.toStdString(),
                        [self = QPointer<InventoryViewModel>(this), page, tokenAtSend, epochAtSend](bool success,
                              const gocook::models::PagedInventory& data,
                              const std::string& error) {
                            if (!self) return;
                            // 会话已切换：过期响应作废。仅当本响应仍属最新代次（无新请求接管、
                            // clearAll 未执行）时复位加载标记防页面卡死；否则状态由新请求/clearAll 管理
                            if (self->m_api->authToken() != tokenAtSend) {
                                if (self->m_epoch == epochAtSend && self->m_isLoading) {
                                    self->m_isLoading = false;
                                    emit self->isLoadingChanged();
                                }
                                return;
                            }
                            // 代次落后：本响应发送后又有更新的请求发出（换词/刷新/clearAll 均会递增）——
                            // 本响应已过时，静默丢弃：不落数据、不动加载标记（最新请求自行收尾）
                            if (epochAtSend != self->m_epoch) return;
                            if (!success) {
                                const QString errorText = QString::fromStdString(error);
                                // 守卫拦截（未登录，error=请先登录）：清空上一登录态残留数据，
                                // 避免游客看到旧库存（页面空态文案另有呈现，状态提示保持原样）
                                if (errorText == HttpGoCookApi::kAuthRequiredError) {
                                    self->m_items.clear();
                                    self->m_hasMore = false;
                                    self->m_currentPage = 1;
                                    self->m_lastLoadFailed = false;
                                    self->m_itemsFromSnapshotFallback = false;
                                    self->stopRetry();
                                    self->clearLoadFailed();
                                    emit self->itemsChanged();
                                    emit self->hasMoreChanged();
                                    emit self->errorOccurred(errorText);
                                } else if (page <= 1) {
                                    // 首页失败：快照静默兜底 / 离线视图 / 退避重试（不再走页内提示——
                                    // 一个失败一个反馈，呈现交给兜底或离线视图）
                                    self->handleFirstPageFailure(errorText);
                                } else {
                                    // 翻页失败：保留现状（列表上方页内提示），不触发兜底与重试
                                    emit self->errorOccurred(errorText);
                                }
                                self->m_isLoading = false;
                                emit self->isLoadingChanged();
                                return;
                            }

                            if (page == 1)
                                self->m_items.clear();

                            for (const auto& item : data.data)
                                self->m_items.append(DataMapper::toMap(item));

                            self->m_currentPage = data.pagination.page;
                            self->m_totalPages = data.pagination.total_pages;
                            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

                            // 在线优先：成功加载即刷新本地快照（仅无过滤首页——过滤态是子集视图、
                            // 后续页是增量，都不能代表"全量首页快照"；缓存尽力而为，失败已由 DB 层打日志）
                            if (page == 1 && self->m_filterText.isEmpty())
                                self->m_db->saveInventoryCache(self->m_items);

                            // 加载成功：清失败态、停退避重试（重拉的目的已达成）；列表回到服务端数据
                            self->m_lastLoadFailed = false;
                            self->m_itemsFromSnapshotFallback = false;
                            self->stopRetry();
                            self->clearLoadFailed();

                            emit self->itemsChanged();
                            emit self->hasMoreChanged();
                            self->m_isLoading = false;
                            emit self->isLoadingChanged();
                        });
}

void InventoryViewModel::addItem(const QString& name, double quantity,
                                  const QString& unit, const QString& expiryDate)
{
    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = name.toStdString();
    req.quantity = quantity;
    req.unit = unit.toStdString();
    if (!expiryDate.isEmpty())
        req.expiry_date = expiryDate.toStdString();

    m_api->upsertInventory(req, [self = QPointer<InventoryViewModel>(this)](bool success, int id, const std::string& error) {
        if (!self) return;
        if (success) {
            self->refresh();
        } else {
            emit self->errorOccurred(QString::fromStdString(error));
        }
    });
}

void InventoryViewModel::updateItem(int itemId, const QString& name, double quantity,
                                     const QString& unit, const QString& expiryDate)
{
    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = name.toStdString();
    req.quantity = quantity;
    req.unit = unit.toStdString();
    if (!expiryDate.isEmpty())
        req.expiry_date = expiryDate.toStdString();

    // 编辑 = 按 id 整行替换（PUT /api/inventory/:id）：改名/改单位/改数量均为替换而非累加
    // （决策 2026-09-04：添加页走 POST 累加，编辑入口走 PUT 替换）
    m_api->updateInventoryItem(itemId, req,
        [self = QPointer<InventoryViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                self->refresh();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void InventoryViewModel::deleteItem(int itemId)
{
    // 乐观删除：保存旧列表用于回滚
    QVariantList oldItems = m_items;

    // 1. 立即从列表中移除（同时取被删行的复合键，供成功回调同步快照行）
    QString removedName;
    QString removedUnit;
    for (int i = 0; i < m_items.size(); ++i) {
        const QVariantMap row = m_items[i].toMap();
        if (row["id"].toInt() == itemId) {
            removedName = row["ingredientName"].toString();
            removedUnit = row["unit"].toString();
            m_items.removeAt(i);
            emit itemsChanged();
            break;
        }
    }

    // 2. 发 API 请求
    m_api->deleteInventoryItem(itemId,
        [self = QPointer<InventoryViewModel>(this), itemId, oldItems, removedName, removedUnit]
        (bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                // 失败 → 回滚（快照本未改动，无需同步）
                self->m_items = oldItems;
                emit self->itemsChanged();
                emit self->errorOccurred(QString::fromStdString(error));
                return;
            }
            // 成功 → 快照行同步移除（防"删除后断网离线兜底复活已删项"）。
            // 行级删除按复合键（ingredient_name + unit）定位，过滤/分页/快照兜底显示态下均精确成立；
            // 行不在快照中时为 no-op（DB 层保证）
            self->m_db->removeInventoryCacheItem(removedName, removedUnit);
        });
}

void InventoryViewModel::onNetworkRestored()
{
    // 联网即同步：仅当数据可能过期（上次首页失败 / 没任何数据可显示）时重拉；
    // 健康数据不打扰——避免用户浏览时列表无谓跳动
    if (m_api->authToken().empty())
        return; // 未登录：无数据可同步
    if (m_isLoading)
        return; // 已有在途请求，其结果即最新
    if (!m_lastLoadFailed && !m_items.isEmpty())
        return;
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::setRetryDelaysMs(const QList<int>& delays)
{
    if (!delays.isEmpty())
        m_retryDelays = delays;
}

void InventoryViewModel::handleFirstPageFailure(const QString& error)
{
    // 首页加载失败：有内存数据 → 静默保留（与联网无异）；无内存数据 → 快照兜底；
    // 快照也不存在 → 置 loadFailed（页面离线视图呈现）
    m_lastLoadFailed = true;
    // 快照兜底条件：内存无数据，或当前列表本就来自快照兜底（过滤词变化/清空后需重新派生，
    // 否则会把"过滤后的旧子集"当作新过滤词的结果展示）
    if (m_items.isEmpty() || m_itemsFromSnapshotFallback) {
        if (m_db->hasInventoryCache()) {
            // 快照存在（可能为空列表——库存本来就是空的）：静默填充，与联网状态无异；
            // 过滤态按当前过滤词本地过滤（近似服务端 ILIKE 子串语义），列表与过滤框保持一致
            m_items = filterSnapshotByKeyword(m_db->getInventoryCache(), m_filterText);
            m_itemsFromSnapshotFallback = true;
            m_currentPage = 1;
            m_totalPages = 1;
            m_hasMore = false;
            emit itemsChanged();
            emit hasMoreChanged();
        } else if (m_items.isEmpty()) {
            // 仅当确实无数据可显示时才进离线态（列表来自快照但快照已删的窗口期不误置）；
            // 文案随失败类型更新（重试再次失败时错误可能变化）
            if (m_loadFailedMessage != error) {
                m_loadFailedMessage = error;
                emit loadFailedMessageChanged();
            }
            if (!m_loadFailed) {
                m_loadFailed = true;
                emit loadFailedChanged();
            }
        }
    }
    // 失败反馈分层：
    //  瞬时故障（网络层错误 / 服务器繁忙 503）→ 静默 + 退避重试（重试即反馈，不打扰）；
    //  非瞬时故障且列表原样保留（有数据非快照态）→ 页内提示——该路径不会自动重试，
    //   用户手动触发后必须知道失败（快照兜底/离线视图场景由对应呈现负责，保持静默）
    const bool transient = (error == HttpGoCookApi::kNetworkErrorMessage ||
                            error == HttpGoCookApi::kServerBusyErrorMessage);
    if (transient) {
        scheduleRetry();
    } else if (!m_items.isEmpty() && !m_itemsFromSnapshotFallback) {
        emit errorOccurred(error);
    }
}

void InventoryViewModel::scheduleRetry()
{
    if (m_retryDelays.isEmpty())
        return;
    const int idx = qMin(m_retryAttempt, m_retryDelays.size() - 1);
    ++m_retryAttempt;
    m_retryTimer.start(m_retryDelays.at(idx));
}

void InventoryViewModel::stopRetry()
{
    m_retryTimer.stop();
    m_retryAttempt = 0;
}

void InventoryViewModel::clearLoadFailed()
{
    if (!m_loadFailed && m_loadFailedMessage.isEmpty())
        return;
    m_loadFailed = false;
    m_loadFailedMessage.clear();
    emit loadFailedChanged();
    emit loadFailedMessageChanged();
}

void InventoryViewModel::onRetryTick()
{
    // 登出后不再重试；否则重拉首页（成功会经回调停表，失败会再排下一档）
    if (m_api->authToken().empty()) {
        stopRetry();
        return;
    }
    // 已有在途请求：不插新请求（新请求会以 epoch 顶掉在途响应、打断用户操作）。
    // 在途为首页加载时其回调自会续排/停表；为翻页失败时不会续排（策略），
    // 故此处补排"当前档位"一档，防重试链在极端交错下悄然断掉
    if (m_isLoading) {
        if (!m_retryDelays.isEmpty()) {
            const int idx = qMin(m_retryAttempt, m_retryDelays.size() - 1);
            m_retryTimer.start(m_retryDelays.at(idx));
        }
        return;
    }
    loadInventory(1, m_pageSize);
}
