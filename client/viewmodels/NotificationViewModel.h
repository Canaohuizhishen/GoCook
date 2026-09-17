#pragma once

#include <QObject>
#include <QVariantList>
#include <QSet>
#include <gocook/IGoCookApi.h>

/**
 * @brief 通知中心 ViewModel：列表加载、已读状态与类型筛选。
 *
 * 所有 Q_INVOKABLE 异步：结果经 Q_PROPERTY + NOTIFY 驱动 QML；失败发 errorOccurred；底层 IGoCookApi。
 *
 * 筛选数据源（currentType）：
 *   ""=全部（公告 + per-user 通知合并，按 createdAt 降序）；"system"=仅公告；其他=服务端类型过滤。
 * 公告的特殊待遇（isAnnouncementItem 判定）：
 *   视为已读、不参与未读计数；markRead 直接跳过（无信号）；markAllRead 在列表无真实通知时整体跳过；
 *   删除仅本地移除（不发网络请求，成功也发 deleteSuccess）。
 */
class NotificationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString currentType READ currentType WRITE setCurrentType NOTIFY currentTypeChanged)   ///< ""=全部 / "system"=公告 / 其他（如 "review"）=类型过滤；赋值即切筛并自动刷新
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)   ///< 未读数：当前列表 is_read=false 计数（公告视为已读）
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)   ///< 刷新态：refresh() 开始置 true，完成回调置 false（QML 下拉刷新指示器）

public:
    explicit NotificationViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QVariantList notifications() const;
    bool isLoading() const;
    bool hasMore() const;
    QString currentType() const;
    int unreadCount() const;
    bool isRefreshing() const;

    /// 切换筛选（类型变化时自动 refresh；取值见 currentType 属性）
    void setCurrentType(const QString& type);

    // 通知操作
    /// 加载第一页（按 currentType 分派数据源）；失败发 errorOccurred。
    Q_INVOKABLE void loadNotifications(int page = 1, int size = 20);
    /// 加载下一页（加载中或 !hasMore 时忽略）。
    Q_INVOKABLE void loadNextPage();
    /// 清空并按当前筛选重载第一页（setCurrentType 会自动调用）。
    Q_INVOKABLE void refresh();
    /// 标记单条已读（公告跳过且无信号）；成功发 markReadSuccess，失败发 errorOccurred。
    Q_INVOKABLE void markRead(int notificationId);
    /// 全部已读（列表无真实通知时整体跳过）；成功发 markAllReadSuccess，失败发 errorOccurred。
    Q_INVOKABLE void markAllRead();
    /// 删除通知（公告仅本地移除）。成功发 deleteSuccess，失败发 errorOccurred。
    Q_INVOKABLE void deleteNotification(int notificationId);
    /// 重置测试数据并自动 refresh（仅开发环境）；失败发 errorOccurred。
    Q_INVOKABLE void resetTestData();
    /// 清空全部数据与状态（登出 / 401 自动登出 / 注销的统一清理，main.cpp 单点接线调用；
    /// 非 Q_INVOKABLE——QML 不再手工逐 VM 清理，避免新增个人域时遗漏）。
    void clearAll();

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；结果信号的触发见对应方法注释。
    void notificationsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void currentTypeChanged();
    void unreadCountChanged();
    void isRefreshingChanged();
    /// 加载 / 操作失败的统一通道（文案可直接展示）。
    void errorOccurred(const QString& error);
    void markReadSuccess(int notificationId);
    void markAllReadSuccess();
    void deleteSuccess(int notificationId);

private:
    /// 从当前列表重算未读数（变化时发 unreadCountChanged）。
    void recalcUnreadCount();
    /// 仅加载公告并映射为通知格式（"system"筛选专用；page=1 先清空）
    void loadAnnouncements(int page, int size);
    /// 仅从通知接口加载并追加（不处理公告；"全部"续页由 needsResort 触发重排）
    void loadNotificationsOnly(int page, int size, bool needsResort = false);
    /// 判断某条通知是否来自系统公告（非 per-user 通知）
    bool isAnnouncementItem(int notificationId) const { return m_announcementIds.contains(notificationId); }

    IGoCookApi *m_api;              ///< API 门面（经 HTTP 实现）
    QVariantList m_notifications;   ///< 当前通知列表
    QSet<int> m_announcementIds;  ///< 跟踪来自公告的数据 ID，跳过删除/标记已读
    bool m_isLoading = false;   ///< 加载请求在途
    bool m_hasMore = false;   ///< 还有下一页
    int m_currentPage = 1;   ///< 当前页
    int m_pageSize = 20;   ///< 每页数量
    int m_totalPages = 0;   ///< 总页数
    QString m_currentType;   ///< 当前筛选（""=全部 / "system"=公告 / 其他=类型过滤）
    int m_unreadCount = 0;   ///< 未读数（从列表重算）
    bool m_isRefreshing = false;   ///< 刷新态（refresh 置真；各完成回调统一置假）
};
