#include "NotificationViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include <algorithm>

NotificationViewModel::NotificationViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList NotificationViewModel::notifications() const { return m_notifications; }
bool NotificationViewModel::isLoading() const { return m_isLoading; }
bool NotificationViewModel::hasMore() const { return m_hasMore; }
QString NotificationViewModel::currentType() const { return m_currentType; }
int NotificationViewModel::unreadCount() const { return m_unreadCount; }
bool NotificationViewModel::isRefreshing() const { return m_isRefreshing; }

void NotificationViewModel::setCurrentType(const QString& type)
{
    if (m_currentType != type) {
        m_currentType = type;
        emit currentTypeChanged();
        refresh();
    }
}

void NotificationViewModel::refresh()
{
    m_currentPage = 1;
    m_notifications.clear();
    m_announcementIds.clear();
    emit notificationsChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadNotifications(1, m_pageSize);
}

void NotificationViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadNotifications(m_currentPage + 1, m_pageSize);
}

void NotificationViewModel::loadNotifications(int page, int size)
{
    // 筛选"系统公告"时只加载公告
    if (m_currentType == QLatin1String("system")) {
        loadAnnouncements(page, size);
        return;
    }

    m_isLoading = true;
    emit isLoadingChanged();

    // "全部"且第一页：加载公告 + 通知，按 createdAt 降序合并
    if (m_currentType.isEmpty() && page == 1) {
        m_notifications.clear();
        m_announcementIds.clear();

        m_api->getAnnouncements(1, 50,
            [self = QPointer<NotificationViewModel>(this), page, size](bool annSuccess,
                  const gocook::models::PagedAnnouncements& annData,
                  const std::string&) {
                if (!self) return;

                // 先收集公告（转为通知格式）
                QVariantList annItems;
                if (annSuccess) {
                    for (const auto& item : annData.data) {
                        self->m_announcementIds.insert(item.id);
                        annItems.append(DataMapper::toNotificationMap(item));
                    }
                }

                // 再加载通知
                std::string emptyFilter;
                self->m_api->getNotifications(page, size, emptyFilter,
                    [self, annItems](bool notifSuccess,
                          const gocook::models::PagedNotifications& notifData,
                          const std::string& error) {
                        if (!self) return;
                        if (!notifSuccess) {
                            // 通知加载失败，至少展示公告
                            emit self->errorOccurred(QString::fromStdString(error));
                        }

                        // 合并
                        QVariantList merged = annItems;
                        if (notifSuccess) {
                            for (const auto& item : notifData.data)
                                merged.append(DataMapper::toMap(item));
                        }

                        // 按 createdAt 降序排列
                        std::sort(merged.begin(), merged.end(),
                            [](const QVariant& a, const QVariant& b) {
                                return a.toMap()["createdAt"].toString()
                                     > b.toMap()["createdAt"].toString();
                            });

                        self->m_notifications = merged;
                        if (notifSuccess) {
                            self->m_currentPage = notifData.pagination.page;
                            self->m_totalPages = notifData.pagination.total_pages;
                            self->m_hasMore = (self->m_currentPage < self->m_totalPages);
                        }
                        self->recalcUnreadCount();
                        emit self->notificationsChanged();
                        emit self->hasMoreChanged();
                        self->m_isLoading = false;
                        emit self->isLoadingChanged();
                        self->m_isRefreshing = false;
                        emit self->isRefreshingChanged();
                    });
            });
        return;
    }

    // 其他标签的第一页：清空列表（移除上个筛选可能留下的公告）
    if (page == 1) {
        m_notifications.clear();
        m_announcementIds.clear();
    }

    // "全部"的后续页：loadNotificationsOnly 追加后需重新排序
    bool needsResort = m_currentType.isEmpty() && page > 1;
    loadNotificationsOnly(page, size, needsResort);
}

void NotificationViewModel::loadNotificationsOnly(int page, int size, bool needsResort)
{
    std::string typeFilter = (m_currentType == QLatin1String("system"))
                             ? std::string()
                             : m_currentType.toStdString();

    m_api->getNotifications(page, size, typeFilter,
        [self = QPointer<NotificationViewModel>(this), page, needsResort](bool success,
              const gocook::models::PagedNotifications& data,
              const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_isLoading = false;
                emit self->isLoadingChanged();
                self->m_isRefreshing = false;
                emit self->isRefreshingChanged();
                return;
            }

            // 注意：page==1 时 m_notifications 已由调用方清空（含公告）
            // 此处不再 clear，避免清掉已加载的公告
            for (const auto& item : data.data)
                self->m_notifications.append(DataMapper::toMap(item));

            // "全部"的后续页：追加后按 createdAt 降序重排
            if (needsResort) {
                std::sort(self->m_notifications.begin(), self->m_notifications.end(),
                    [](const QVariant& a, const QVariant& b) {
                        return a.toMap()["createdAt"].toString()
                             > b.toMap()["createdAt"].toString();
                    });
            }

            self->m_currentPage = data.pagination.page;
            self->m_totalPages = data.pagination.total_pages;
            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

            self->recalcUnreadCount();

            emit self->notificationsChanged();
            emit self->hasMoreChanged();
            self->m_isLoading = false;
            emit self->isLoadingChanged();
            self->m_isRefreshing = false;
            emit self->isRefreshingChanged();
        });
}

void NotificationViewModel::loadAnnouncements(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getAnnouncements(page, size,
        [self = QPointer<NotificationViewModel>(this), page](bool success,
              const gocook::models::PagedAnnouncements& data,
              const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_isLoading = false;
                emit self->isLoadingChanged();
                self->m_isRefreshing = false;
                emit self->isRefreshingChanged();
                return;
            }

            if (page == 1) {
                self->m_notifications.clear();
                self->m_announcementIds.clear();
            }

            for (const auto& item : data.data) {
                self->m_announcementIds.insert(item.id);
                self->m_notifications.append(DataMapper::toNotificationMap(item));
            }

            self->m_currentPage = data.pagination.page;
            self->m_totalPages = data.pagination.total_pages;
            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

            // 公告全视为已读，不触发 recalcUnreadCount
            self->recalcUnreadCount();

            emit self->notificationsChanged();
            emit self->hasMoreChanged();
            self->m_isLoading = false;
            emit self->isLoadingChanged();
            self->m_isRefreshing = false;
            emit self->isRefreshingChanged();
        });
}

void NotificationViewModel::markRead(int notificationId)
{
    // 系统公告没有已读状态，跳过
    if (isAnnouncementItem(notificationId)) {
        return;
    }

    m_api->markNotificationRead(notificationId,
        [self = QPointer<NotificationViewModel>(this), notificationId](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                // 更新本地列表中的 is_read 状态
                for (int i = 0; i < self->m_notifications.size(); ++i) {
                    auto item = self->m_notifications[i].toMap();
                    if (item["id"].toInt() == notificationId) {
                        item["is_read"] = true;
                        self->m_notifications[i] = item;
                        break;
                    }
                }
                self->recalcUnreadCount();
                emit self->notificationsChanged();
                emit self->markReadSuccess(notificationId);
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void NotificationViewModel::markAllRead()
{
    // 如果当前列表中只有系统公告，跳过
    {
        bool hasRealNotifications = false;
        for (const auto& v : m_notifications) {
            if (!isAnnouncementItem(v.toMap()["id"].toInt())) {
                hasRealNotifications = true;
                break;
            }
        }
        if (!hasRealNotifications) return;
    }

    m_api->markAllNotificationsRead(
        [self = QPointer<NotificationViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                // 本地全部标记为已读
                for (int i = 0; i < self->m_notifications.size(); ++i) {
                    auto item = self->m_notifications[i].toMap();
                    item["is_read"] = true;
                    self->m_notifications[i] = item;
                }
                self->m_unreadCount = 0;
                emit self->notificationsChanged();
                emit self->unreadCountChanged();
                emit self->markAllReadSuccess();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void NotificationViewModel::deleteNotification(int notificationId)
{
    // 系统公告无法由用户删除，直接从本地列表移除
    if (isAnnouncementItem(notificationId)) {
        QVariantList updated;
        for (const auto& v : m_notifications) {
            if (v.toMap()["id"].toInt() == notificationId) continue;
            updated.append(v);
        }
        m_notifications = updated;
        emit notificationsChanged();
        emit deleteSuccess(notificationId);
        return;
    }

    m_api->deleteNotification(notificationId,
        [self = QPointer<NotificationViewModel>(this), notificationId](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                // 从本地列表中移除
                QVariantList updated;
                bool removedUnread = false;
                for (const auto& v : self->m_notifications) {
                    auto item = v.toMap();
                    if (item["id"].toInt() == notificationId) {
                        if (!item["is_read"].toBool())
                            removedUnread = true;
                        continue;
                    }
                    updated.append(v);
                }
                self->m_notifications = updated;
                if (removedUnread) {
                    self->recalcUnreadCount();
                }
                emit self->notificationsChanged();
                emit self->deleteSuccess(notificationId);
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void NotificationViewModel::resetTestData()
{
    m_api->resetTestNotifications([self = QPointer<NotificationViewModel>(this)](bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->refresh();
        } else {
            emit self->errorOccurred(QString::fromStdString(error));
        }
    });
}

void NotificationViewModel::recalcUnreadCount()
{
    int count = 0;
    for (const auto& v : m_notifications) {
        auto item = v.toMap();
        if (!item["is_read"].toBool())
            count++;
    }
    if (m_unreadCount != count) {
        m_unreadCount = count;
        emit unreadCountChanged();
    }
}
