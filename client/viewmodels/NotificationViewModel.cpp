#include "NotificationViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include <algorithm>
#include "RequestGuards.h"

NotificationViewModel::NotificationViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

const NotificationViewModel::CategoryState& NotificationViewModel::stateFor(const QString& type) const
{
    return (type == QLatin1String("interaction")) ? m_interaction : m_review;
}

NotificationViewModel::CategoryState& NotificationViewModel::stateFor(const QString& type)
{
    return (type == QLatin1String("interaction")) ? m_interaction : m_review;
}

QVariantList NotificationViewModel::notifications() const { return stateFor(m_currentType).items; }
bool NotificationViewModel::isLoading() const { return m_pendingLoads > 0; }
bool NotificationViewModel::hasMore() const { return stateFor(m_currentType).hasMore; }
bool NotificationViewModel::isRefreshing() const { return m_isRefreshing; }
QString NotificationViewModel::currentType() const { return m_currentType; }
int NotificationViewModel::reviewUnread() const { return m_reviewUnread; }
int NotificationViewModel::interactionUnread() const { return m_interactionUnread; }
int NotificationViewModel::unreadTotal() const { return m_reviewUnread + m_interactionUnread; }
bool NotificationViewModel::systemHasNew() const { return m_systemHasNew; }

void NotificationViewModel::beginLoad()
{
    if (m_pendingLoads++ == 0)
        emit isLoadingChanged();
}

void NotificationViewModel::endLoad()
{
    if (--m_pendingLoads == 0)
        emit isLoadingChanged();
}

void NotificationViewModel::setCurrentType(const QString& type)
{
    if (m_currentType == type) return;
    m_currentType = type;
    emit currentTypeChanged();
    emit notificationsChanged();   // 列表数据源切换
    emit hasMoreChanged();

    // 惰性加载：目标分类本次入页尚未加载过 → 拉第一页（进入分类 = 已读）
    if (!stateFor(type).loaded)
        loadCategory(type, 1, m_pageSize, false);
}

void NotificationViewModel::refresh()
{
    // 进入页面 / 下拉刷新：两类均置过期（退出重进刷新语义），重载当前分类第一页
    m_review.loaded = false;
    m_interaction.loaded = false;

    // 刷新态：QML 下拉刷新指示器用。
    if (!m_isRefreshing) {
        m_isRefreshing = true;
        emit isRefreshingChanged();
    }

    // 重载当前分类第一页。
    loadCategory(m_currentType, 1, m_pageSize, true);
}

void NotificationViewModel::loadNotifications(int page, int size)
{
    m_pageSize = size;
    loadCategory(m_currentType, page, size, false);
}

void NotificationViewModel::loadNextPage()
{
    const CategoryState& st = stateFor(m_currentType);
    if (m_pendingLoads > 0 || !st.hasMore) return;
    loadCategory(m_currentType, st.page + 1, m_pageSize, false);
}

void NotificationViewModel::loadCategory(const QString& type, int page, int size, bool isRefreshCall)
{
    // 在途加载计数 +1。
    beginLoad();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应作废
    const auto session = SessionSnapshot::capture(m_api);

    if (page == 1) {
        // 第一页 = 首屏替换：清空该分类缓存（双分类各自独立，不触碰另一分类）
        stateFor(type).items.clear();
        if (m_currentType == type)
            emit notificationsChanged();
    }

    m_api->getNotifications(page, size, type.toStdString(),
        [self = QPointer<NotificationViewModel>(this), session, type, page, isRefreshCall]
        (bool success, const gocook::models::PagedNotifications& data, const std::string& error) {
            // QPointer 悬垂守卫：ViewModel 已销毁，直接退出。
            if (!self) return;

            // 与 beginLoad 配对，在途计数 -1。
            self->endLoad();

            // 如果是 refresh() 触发的加载，完成时复位刷新态。
            // 同样无论成功失败、会话是否有效都要复位，防止刷新指示器卡住。
            if (isRefreshCall) {
                self->m_isRefreshing = false;
                emit self->isRefreshingChanged();
            }

            // 会话已切换：过期响应作废
            if (!session.isCurrent(self->m_api)) return;

            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                return;
            }

            // 成功：取该分类的状态引用（review 或 interaction）。
            CategoryState& st = self->stateFor(type);

            // 追加本页条目。
            // page == 1 时上面已经清空过，所以这里是“替换后追加”；
            // page > 1 时是续页追加。
            for (const auto& item : data.data)
                st.items.append(DataMapper::toMap(item));

            // 更新分页元数据
            st.page = data.pagination.page;
            st.totalPages = data.pagination.total_pages;
            st.hasMore = st.page < st.totalPages;

            // 标记该分类本次入页已加载过。
            // setCurrentType() 的惰性加载据此判断：!loaded 才拉第一页。
            st.loaded = true;

            // 进入分类 = 已读：本地去点 + 计数清零 + 上报水位（仅第一页语义）
            if (page == 1)
                self->reportSeenAndClear(type, data);

            // 如果当前正显示这个分类，通知列表与 hasMore 变化。
            if (self->m_currentType == type) {
                emit self->notificationsChanged();
                emit self->hasMoreChanged();
            }
        });
}

void NotificationViewModel::reportSeenAndClear(const QString& type,
                                               const gocook::models::PagedNotifications& data)
{
    // 取本页最大 id 作已读水位。
    // 不用 front().id：那假设服务端按 id 降序，排序一变就静默上报偏小水位。
    // 一页最多 20 条，遍历成本可忽略，换对排序零假设。空页 maxId=0，后面直接 return。
    int maxId = 0;
    for (const auto& item : data.data)
        maxId = std::max(maxId, item.id);

    // 本地即时表现：已加载条目的未读小点抹平 + 该分类计数清零
    // （注意：这只是乐观 UI，服务端才是权威；上报失败静默，下一轮汇总会自愈）
    CategoryState& st = stateFor(type);
    bool dotsChanged = false;
    for (int i = 0; i < st.items.size(); ++i) {
        QVariantMap item = st.items[i].toMap();
        if (!item["is_read"].toBool()) {
            item["is_read"] = true;
            st.items[i] = item;
            dotsChanged = true;
        }
    }

    // 只有当前正显示该分类时才发列表变化信号，
    // 避免无谓刷新另一个分类的 UI。
    if (dotsChanged && m_currentType == type)
        emit notificationsChanged();

    // 该分类未读计数清零。
    // 本地即时反馈；服务端汇总后续会校正。
    setUnread(type, 0);

    // 上报水位。
    // 如果本页没有有效 id，或 maxId 没有超过本地已成功上报的水位，
    // 就不发请求——避免重复上报。
    if (maxId <= 0)
        return;
    if (maxId <= m_reportedWatermark.value(type, 0))
        return;

    // 再捕获一次会话快照
    const auto session = SessionSnapshot::capture(m_api);

    m_api->setNotificationsReadState(type.toStdString(), maxId,
        [self = QPointer<NotificationViewModel>(this), session, type, maxId]
        (bool success, const std::string&) {
            if (!self) return;

            // 会话已切换：丢弃过期响应。
            if (!session.isCurrent(self->m_api)) return;

            // 只有成功且 maxId 仍大于本地已上报值时，才更新本地水位。
            // 失败静默：下次进入分类会重报；
            // 服务端保证水位不会回退。
            if (success && maxId > self->m_reportedWatermark.value(type, 0))
                self->m_reportedWatermark[type] = maxId;
        });
}

void NotificationViewModel::setUnread(const QString& type, int value)
{
    value = std::max(0, value);
    if (type == QLatin1String("interaction")) {
        if (m_interactionUnread == value) return;
        m_interactionUnread = value;
    } else {
        if (m_reviewUnread == value) return;
        m_reviewUnread = value;
    }
    emit unreadSummaryChanged();
}

void NotificationViewModel::adjustUnread(const QString& type, int delta)
{
    const int base = (type == QLatin1String("interaction")) ? m_interactionUnread : m_reviewUnread;
    setUnread(type, base + delta);
}

void NotificationViewModel::refreshUnreadSummary()
{
    // 快照当前会话：换号后的旧账号汇总不得落入新账号角标
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getNotificationsUnreadSummary(
        [self = QPointer<NotificationViewModel>(this), session]
        (bool success, const gocook::models::NotificationUnreadSummary& data, const std::string&) {
            if (!self) return;
            if (!session.isCurrent(self->m_api)) return;
            if (!success) return;   // 失败静默：保留旧缓存/空值，不打断用户

            // 用 changed 标记是否有任何字段变化，只有变化才发信号，避免无谓的 QML 绑定刷新。
            bool changed = false;

            // 三种类型的未读数都应以服务端为准。
            // 本地删除未读条目（异常情况下）时可能已做过 -1 即时反馈，这里会被服务端权威值覆盖校正。
            if (self->m_reviewUnread != data.unread_review) {
                self->m_reviewUnread = data.unread_review;
                changed = true;
            }
            if (self->m_interactionUnread != data.unread_interaction) {
                self->m_interactionUnread = data.unread_interaction;
                changed = true;
            }
            if (self->m_systemHasNew != data.has_new_announcement) {
                self->m_systemHasNew = data.has_new_announcement;
                changed = true;
            }

            if (changed)
                emit self->unreadSummaryChanged();
        });
}

void NotificationViewModel::deleteNotification(int notificationId)
{
    // 快照当前会话：换号后的旧账号删除响应不得改动新账号数据
    const auto session = SessionSnapshot::capture(m_api);
    m_api->deleteNotification(notificationId,
        [self = QPointer<NotificationViewModel>(this), session, notificationId]
        (bool success, const std::string& error) {
            if (!self) return;
            if (!session.isCurrent(self->m_api)) return;

            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                return;
            }

            // 从两个分类缓存中定位并移除（条目归属由 id 反查，不依赖当前分类）
            QString removedFrom;
            bool wasUnread = false;
            for (const QString& t : {QStringLiteral("review"), QStringLiteral("interaction")}) {
                CategoryState& st = self->stateFor(t);
                QVariantList kept;
                kept.reserve(st.items.size());
                bool found = false;
                for (const auto& v : st.items) {
                    const QVariantMap item = v.toMap();
                    if (item["id"].toInt() == notificationId) {
                        found = true;
                        wasUnread = !item["is_read"].toBool();
                        continue;
                    }
                    kept.append(v);
                }
                if (found) {
                    st.items = kept;
                    removedFrom = t;
                    break;
                }
            }
            if (!removedFrom.isEmpty()) {
                if (self->m_currentType == removedFrom)
                    emit self->notificationsChanged();
                // 删未读条目（异常状况下）：该分类计数即时 -1（下限 0；汇总仍为权威，下次拉取校正）
                if (wasUnread)
                    self->adjustUnread(removedFrom, -1);
            }
            emit self->deleteSuccess(notificationId);
        });
}

void NotificationViewModel::clearSystemNewFlag()
{
    if (!m_systemHasNew) return;
    m_systemHasNew = false;
    emit unreadSummaryChanged();
}

void NotificationViewModel::resetTestData()
{
    m_api->resetTestNotifications([self = QPointer<NotificationViewModel>(this)](bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->refresh();
            self->refreshUnreadSummary();
        } else {
            emit self->errorOccurred(QString::fromStdString(error));
        }
    });
}

void NotificationViewModel::clearAll()
{
    // 登出统一清理（main.cpp 单点接线）：双分类缓存 / 汇总角标 / 已上报水位 / 刷新态全部归零。
    // 刻意不动 m_pendingLoads：在途请求的回调仍会调用 endLoad 配对计数，清掉会负漂移
    m_review = CategoryState{};
    m_interaction = CategoryState{};
    m_currentType = QStringLiteral("review");
    m_reviewUnread = 0;
    m_interactionUnread = 0;
    m_systemHasNew = false;
    m_reportedWatermark.clear();
    m_isRefreshing = false;
    emit notificationsChanged();
    emit hasMoreChanged();
    emit currentTypeChanged();
    emit unreadSummaryChanged();
    emit isRefreshingChanged();
}
