#pragma once

#include <QObject>
#include <QVariantList>
#include <gocook/IGoCookApi.h>

class NotificationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString currentType READ currentType WRITE setCurrentType NOTIFY currentTypeChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)

public:
    explicit NotificationViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList notifications() const;
    bool isLoading() const;
    bool hasMore() const;
    QString currentType() const;
    int unreadCount() const;
    bool isRefreshing() const;

    void setCurrentType(const QString& type);

    Q_INVOKABLE void loadNotifications(int page = 1, int size = 20);
    Q_INVOKABLE void loadNextPage();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void markRead(int notificationId);
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE void deleteNotification(int notificationId);
    Q_INVOKABLE void resetTestData();

signals:
    void notificationsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void currentTypeChanged();
    void unreadCountChanged();
    void isRefreshingChanged();
    void errorOccurred(const QString& error);
    void markReadSuccess(int notificationId);
    void markAllReadSuccess();
    void deleteSuccess(int notificationId);

private:
    void recalcUnreadCount();

    IGoCookApi *m_api;
    QVariantList m_notifications;
    bool m_isLoading = false;
    bool m_hasMore = false;
    int m_currentPage = 1;
    int m_pageSize = 20;
    int m_totalPages = 0;
    QString m_currentType;
    int m_unreadCount = 0;
    bool m_isRefreshing = false;
};
