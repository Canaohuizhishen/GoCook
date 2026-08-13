#pragma once

#include <QObject>
#include <QVariantList>
#include <QSet>
#include <gocook/IGoCookApi.h>

/**
 * @brief 通知中心 ViewModel：管理通知列表、已读状态与类型筛选。
 *
 * 所有 Q_INVOKABLE 均为异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新，
 * 失败经 errorOccurred 信号通知 QML；底层统一调用 IGoCookApi。
 * 当筛选类型为 "system" 时，从公告接口加载数据并映射为通知格式。
 */
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

    // 通知操作
    // 加载第一页通知
    Q_INVOKABLE void loadNotifications(int page = 1, int size = 20);
    // 加载下一页通知
    Q_INVOKABLE void loadNextPage();
    // 重新加载当前类型的第一页
    Q_INVOKABLE void refresh();
    // 标记单条通知已读
    Q_INVOKABLE void markRead(int notificationId);
    // 全部标记已读
    Q_INVOKABLE void markAllRead();
    // 删除通知
    Q_INVOKABLE void deleteNotification(int notificationId);
    // 重置测试数据（仅开发环境）
    Q_INVOKABLE void resetTestData();

signals:
    // 数据变更
    void notificationsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void currentTypeChanged();
    void unreadCountChanged();
    void isRefreshingChanged();
    // 操作结果
    void errorOccurred(const QString& error);
    void markReadSuccess(int notificationId);
    void markAllReadSuccess();
    void deleteSuccess(int notificationId);

private:
    // 重新统计未读数量
    void recalcUnreadCount();
    /// 当筛选类型为"system"时，从公告接口加载数据并映射为通知格式
    void loadAnnouncements(int page, int size);
    /// 仅从通知接口加载（不处理公告），供"全部"标签链式调用
    void loadNotificationsOnly(int page, int size, bool needsResort = false);
    /// 判断某条通知是否来自系统公告（非 per-user 通知）
    bool isAnnouncementItem(int notificationId) const { return m_announcementIds.contains(notificationId); }

    IGoCookApi *m_api;              ///< API 门面（经 HTTP 实现）
    QVariantList m_notifications;   ///< 当前通知列表
    QSet<int> m_announcementIds;  ///< 跟踪来自公告的数据 ID，跳过删除/标记已读
    bool m_isLoading = false;
    bool m_hasMore = false;
    int m_currentPage = 1;
    int m_pageSize = 20;
    int m_totalPages = 0;
    QString m_currentType;
    int m_unreadCount = 0;
    bool m_isRefreshing = false;
};
