#include "NotificationViewModel.h"
#include <DataMapper.h>
#include <QPointer>

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
    m_isLoading = true;
    emit isLoadingChanged();

    std::string typeFilter = m_currentType.toStdString();

    m_api->getNotifications(page, size, typeFilter,
        [self = QPointer<NotificationViewModel>(this), page](bool success,
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

            if (page == 1)
                self->m_notifications.clear();

            for (const auto& item : data.data)
                self->m_notifications.append(DataMapper::toMap(item));

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

void NotificationViewModel::markRead(int notificationId)
{
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
