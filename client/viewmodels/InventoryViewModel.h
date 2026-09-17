#pragma once

#include <QObject>
#include <QVariantList>
#include <QList>
#include <QTimer>
#include <gocook/IGoCookApi.h>
#include "RequestGuards.h"

class LocalDatabase;

/**
 * @brief 库存域 ViewModel：管理库存列表的加载、翻页与增删改。
 *
 * 底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
 * 所有 Q_INVOKABLE 异步（立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML）；
 * 失败分通道：操作类走 errorOccurred，加载类走"快照静默兜底 / 离线视图 / 退避重试"分层呈现（见下）。
 *
 * 并发/异常四道防线（loadInventory 是交汇点，行为由 test_inventory_vm.cpp 锁定；
 * 前两道的判定统一由 RequestGuards.h 值类型承载，见该文件“语义三分”）：
 *   1. 换词竞态   —— 请求代次（RequestEpoch）：换词/刷新/clearAll 作废在途，旧响应静默丢弃；
 *   2. 跨账号串台 —— 会话快照（SessionSnapshot）：发送时捕获 token，响应回来已切换则丢弃；
 *   3. 断网无数据 —— 快照兜底：无快照才进离线视图；
 *   4. 瞬时故障   —— 退避重试：仅网络层错误/503 触发。
 *
 * 离线策略（在线优先 + 联网即同步）：
 *   - 每次成功加载（首页、无过滤）把列表写成库存快照（LocalDatabase，按用户隔离）；
 *   - 首页加载失败且无内存数据时读快照静默兜底（与联网状态无异，零提示）：
 *     过滤态按当前过滤词本地过滤（近似服务端 ILIKE 子串语义）；列表来自快照时，
 *     过滤词变化/清空且再失败会从快照重新派生（不回滚展示旧过滤词子集）；
 *   - 无快照可兜时置 loadFailed（页面居中离线视图 + 手动刷新）；
 *   - 网络恢复（onNetworkRestored）或退避重试到点时，若"上次加载失败 / 无数据"则自动重拉；
 *     退避重试仅覆盖瞬时故障（网络层错误 / 服务器繁忙 503）。
 */
class InventoryViewModel : public QObject
{
    Q_OBJECT

public:
    /// 派生视图状态（对外收敛 QML 组合判断；内部旗标保留）——判定顺序与页面原组合表达式一一对应：
    ///   Offline（失败且无数据，优先：重试在途时离线视图保持稳定不闪烁）→ InitialLoading（空白加载中）
    ///   → Empty（空态）→ Content（有数据）
    enum class ViewState { InitialLoading, Empty, Offline, Content };
    Q_ENUM(ViewState)
    Q_PROPERTY(ViewState viewState READ viewState NOTIFY viewStateChanged)

    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    /// 库存页过滤词（v2.14）：非空时列表请求携带 keyword 服务端过滤；由过滤框防抖后写入
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    /// 首页加载失败且无任何可显示数据（无内存、无快照）——页面居中离线视图
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
    /// 离线视图文案（按失败类型透传：网络层错误 / 服务器繁忙 / 其它服务端文案）
    Q_PROPERTY(QString loadFailedMessage READ loadFailedMessage NOTIFY loadFailedMessageChanged)

public:
    /// db：本地快照存储。生产传 nullptr（使用单例 LocalDatabase::instance()）；测试注入独立内存库
    explicit InventoryViewModel(IGoCookApi *api, QObject *parent = nullptr, LocalDatabase *db = nullptr);

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QVariantList items() const;
    bool isLoading() const;
    bool hasMore() const;
    QString filterText() const;
    bool loadFailed() const;
    QString loadFailedMessage() const;
    /// 派生视图状态（语义见上方枚举注释；QML 经 viewState 收敛原组合判断）
    ViewState viewState() const;
    /// 设置过滤词（trim 后生效）：变化时重置到第一页并按新词加载。
    /// Q_INVOKABLE：QML 过滤框防抖后直接调用（Q_PROPERTY WRITE 只支持属性赋值，
    /// 不会生成 QML 可调用的 setFilterText 函数——曾致 InventoryPage 运行时 TypeError）
    Q_INVOKABLE void setFilterText(const QString& text);

    // 库存操作
    /// 加载第一页库存（开启新一轮请求：作废全部在途请求并立即发送，见 m_epoch）
    Q_INVOKABLE void loadInventory(int page = 1, int size = 50);
    /// 加载下一页（加载中或 !hasMore 时忽略）
    Q_INVOKABLE void loadNextPage();
    /// 添加库存项（expiryDate 为空=无保质期；成功后重载列表）
    Q_INVOKABLE void addItem(const QString& name, double quantity, const QString& unit,
                             const QString& expiryDate = "");
    /// 删除库存项（乐观移除并回滚；成功后同步删除快照行，防离线兜底复活）
    Q_INVOKABLE void deleteItem(int itemId);
    /// 更新库存项（PUT 整行替换，非累加）
    Q_INVOKABLE void updateItem(int itemId, const QString& name, double quantity,
                                const QString& unit, const QString& expiryDate = "");
    /// 重载列表（回到第一页并清空当前数据）
    Q_INVOKABLE void refresh();
    /// 清空全部数据与加载状态（登出/账号切换时调用，杜绝上一账号残留数据串台）
    Q_INVOKABLE void clearAll();
    /// 网络恢复（main.cpp 接 HttpGoCookApi::networkRestored）：数据可能过期（上次加载失败/无数据）时自动重拉
    void onNetworkRestored();
    /// 退避重试间隔（毫秒，默认 5s→10s→20s→30s 封顶；测试注入短间隔用）
    void setRetryDelaysMs(const QList<int>& delays);

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；其余信号单独注明。
    void itemsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    /// 过滤词变化（清空/切换账号时 UI 需同步回显）
    void filterTextChanged();
    /// 离线视图状态变化（loadFailed 显隐）
    void loadFailedChanged();
    /// 离线视图文案变化（失败类型变化 / 退出离线态清空）
    void loadFailedMessageChanged();
    /// 派生 viewState 变化（items / isLoading / loadFailed 任一依赖信号转发，见构造函数）
    void viewStateChanged();
    /// 操作失败（携带错误描述）
    void errorOccurred(const QString& error);

private:
    /// 首页加载失败善后：快照静默兜底（过滤态按词本地过滤/重派生）→ 无快照则置 loadFailed；
    /// 瞬时故障（网络层/503）静默并启动退避重试；非瞬时且数据原样保留时发页内提示
    void handleFirstPageFailure(const QString& error);
    /// 退出离线态（清 loadFailed + 文案；clearAll / 401 守卫 / 加载成功三处统一收口）
    void clearLoadFailed();
    /// 退避重试：排下一档（仅瞬时故障触发——网络层错误 / 服务器繁忙 503；成功/clearAll/登出即停）
    void scheduleRetry();
    /// 停止退避并复位档位（成功 / clearAll / 登出时调用）
    void stopRetry();
    /// 重试定时器到点：无 token 即停；有在途补排当前档（防重试链断）；否则重拉首页
    void onRetryTick();

    IGoCookApi *m_api;         ///< API 门面（经 HTTP 实现）
    LocalDatabase *m_db;       ///< 本地快照存储（测试注入独立库；生产为单例）
    QVariantList m_items;      ///< 当前页库存数据
    QString m_filterText;      ///< 过滤词（空 = 不过滤）
    bool m_isLoading = false;  ///< 是否正在加载
    bool m_hasMore = false;    ///< 是否还有下一页
    bool m_loadFailed = false; ///< 首页加载失败且无任何可显示数据（页面离线视图）
    QString m_loadFailedMessage; ///< 离线视图文案（失败类型透传；退出离线态清空）
    bool m_lastLoadFailed = false; ///< 最近一次首页尝试失败过（可能仍显示旧数据/快照，供自动重拉判定）
    bool m_itemsFromSnapshotFallback = false; ///< 当前列表来自快照兜底（过滤词变化/清空时需从快照重新派生）
    int m_currentPage = 1;     ///< 当前页码
    int m_pageSize = 50;       ///< 每页数量
    int m_totalPages = 0;      ///< 总页数
    QTimer m_retryTimer;       ///< 退避重试定时器（仅失败态运行）
    QList<int> m_retryDelays = {5000, 10000, 20000, 30000}; ///< 退避序列（到末尾后按末尾间隔继续）
    int m_retryAttempt = 0;    ///< 当前退避档位（成功/复位时归零）
    /// 请求代次（RequestGuards.h::RequestEpoch）：新一轮首屏请求（换词/刷新）begin 取票据；
    /// 响应带回票据，到达时已失效即属过期（被更新的请求取代），静默丢弃不落数据不改状态
    RequestEpoch m_epoch;
};
