#pragma once

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <gocook/IGoCookApi.h>

/**
 * @brief 通知中心 ViewModel：双分类（审核结果 / 互动提醒）列表 + 未读角标汇总（v2.23 水位模型）。
 *
 * 核心语义（对齐 api-spec 3.12）：
 *   1. 「进入分类页即已读」：分类第一页加载成功 → 上报该分类当前最大通知 id（水位）→ 服务端未读清零；
 *      本地同时把已加载条目的未读小点抹平并把该分类计数清零（乐观表现，服务端为权威）。
 *   2. 未读数由服务端汇总驱动（refreshUnreadSummary），本地仅在删除未读条目时做 -1 即时反馈；
 *      汇总失败静默保留旧值——角标更新永不打断用户。
 *   3. 无「全部已读」按钮、无单条标读：打开即已读，水位上报收敛阅读语义。
 *
 * 双分类状态各自独立（列表 / 分页 / 是否已加载）：切换不重拉，退出重进刷新（refresh 将两类标记过期）。
 */
class NotificationViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY notificationsChanged)   ///< 当前选中分类的列表
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)   ///< 有加载请求在途（计数派生）
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)   ///< 当前分类还有下一页
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)   ///< 下拉刷新态（QML 指示器）
    Q_PROPERTY(QString currentType READ currentType WRITE setCurrentType NOTIFY currentTypeChanged)   ///< "review"=审核结果 / "interaction"=互动提醒
    Q_PROPERTY(int reviewUnread READ reviewUnread NOTIFY unreadSummaryChanged)   ///< 审核结果未读数（汇总值；删除未读条目时本地 -1）
    Q_PROPERTY(int interactionUnread READ interactionUnread NOTIFY unreadSummaryChanged)   ///< 互动提醒未读数
    Q_PROPERTY(int unreadTotal READ unreadTotal NOTIFY unreadSummaryChanged)   ///< 可计数未读总数（review+interaction；铃铛数字用，不含系统红点）
    Q_PROPERTY(bool systemHasNew READ systemHasNew NOTIFY unreadSummaryChanged)   ///< 系统通知是否有新内容（公告红点；由系统通知页上报后清除）

public:
    explicit NotificationViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QVariantList notifications() const;
    bool isLoading() const;   ///< 特殊实现：在途加载计数派生
    bool hasMore() const;
    bool isRefreshing() const;
    QString currentType() const;
    int reviewUnread() const;
    int interactionUnread() const;
    int unreadTotal() const;
    bool systemHasNew() const;

    /// 切换分类（取值 "review"/"interaction"；目标分类本次尚未加载时自动拉第一页——进入即已读）。
    void setCurrentType(const QString& type);

    // 列表操作
    /// 加载指定分类第一页（进入分类 = 已读：成功上报水位并清零该分类计数）；失败发 errorOccurred。
    Q_INVOKABLE void loadNotifications(int page = 1, int size = 20);
    /// 加载当前分类下一页（加载中或 !hasMore 时忽略；续页上报幂等，不重算计数）。
    Q_INVOKABLE void loadNextPage();
    /// 进入页面 / 下拉刷新：两类均标记过期，重载当前分类第一页（刷新成功同样视为已读）。
    Q_INVOKABLE void refresh();
    /// 拉取未读汇总（进消息页 / 登录成功两处触发；失败静默保留旧值）。
    Q_INVOKABLE void refreshUnreadSummary();
    /// 删除通知：成功本地移除该条目并发 deleteSuccess；被删条目未读时该分类计数 -1（下限 0）。
    Q_INVOKABLE void deleteNotification(int notificationId);
    /// 清除系统通知红点（系统通知页完成"看过"上报后由 main.cpp 单点接线调用；幂等）。
    Q_INVOKABLE void clearSystemNewFlag();
    /// 重置测试数据并刷新（仅开发环境）；失败发 errorOccurred。
    Q_INVOKABLE void resetTestData();
    /// 清空全部数据与状态（登出 / 401 自动登出 / 注销的统一清理，main.cpp 单点接线调用；
    /// 含汇总缓存与已上报水位清空——换号后重新汇总，杜绝跨账号残留）。
    void clearAll();

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；结果信号的触发见对应方法注释。
    void notificationsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void isRefreshingChanged();
    void currentTypeChanged();
    /// 四个角标属性（reviewUnread/interactionUnread/unreadTotal/systemHasNew）任一变化
    void unreadSummaryChanged();
    /// 加载 / 删除失败的统一通道（文案可直接展示）。
    void errorOccurred(const QString& error);
    void deleteSuccess(int notificationId);

private:
    /// 分类列表状态（review / interaction 各一份：切换不重拉，退出重进刷新）
    struct CategoryState {
        QVariantList items;      ///< 已加载条目（page 递增追加）
        int page = 1;            ///< 已加载到第几页
        int totalPages = 0;      ///< 服务端总页数
        bool hasMore = false;    ///< 是否还有下一页
        bool loaded = false;     ///< 本次入页内是否已加载过（惰性加载依据；refresh 时两类一并置否）
    };

    const CategoryState& stateFor(const QString& type) const;
    CategoryState& stateFor(const QString& type);

    /// 加载计数 +1（isLoading 据此翻转）
    void beginLoad();
    /// 加载计数 -1（isLoading 据此翻转）
    void endLoad();

    /// 拉取指定分类（page/size；page==1 = 进入分类：成功上报水位并清零计数）
    void loadCategory(const QString& type, int page, int size, bool isRefreshCall);
    /// 第一页成功后：本地去点 + 计数清零 + 上报水位（> 已上报值才发；失败静默、下次进页重报）
    void reportSeenAndClear(const QString& type, const gocook::models::PagedNotifications& data);
    /// 设置某分类未读数（下限 0；变化才发 unreadSummaryChanged）
    void setUnread(const QString& type, int value);
    /// 某分类未读数增量调整（删除未读条目用）
    void adjustUnread(const QString& type, int delta);

    IGoCookApi *m_api;   ///< API 门面（构造注入）
    QString m_currentType = QStringLiteral("review");   ///< 当前选中分类（默认审核结果）
    CategoryState m_review;          ///< 审核结果分类状态
    CategoryState m_interaction;     ///< 互动提醒分类状态
    int m_pendingLoads = 0;          ///< 在途加载计数（isLoading 据此翻转）
    int m_pageSize = 20;             ///< 每页数量（loadNotifications 传入值）
    bool m_isRefreshing = false;     ///< 刷新态（refresh 置真；完成回调置假）

    // 汇总（服务端权威值；本地仅在删除未读条目时即时 -1）
    int m_reviewUnread = 0;          ///< 审核结果未读数
    int m_interactionUnread = 0;     ///< 互动提醒未读数
    bool m_systemHasNew = false;     ///< 系统通知红点（公告布尔标记）

    QHash<QString, int> m_reportedWatermark;   ///< 分类 → 本会话已成功上报的水位（防重复上报）
};
