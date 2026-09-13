#pragma once

#include <QObject>
#include <QVariantList>
#include <QList>
#include <QTimer>
#include <gocook/IGoCookApi.h>

class LocalDatabase;

/**
 * @brief 库存域 ViewModel：管理库存列表的加载、翻页与增删改。
 *
 * 所有 Q_INVOKABLE 均为异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新，
 * 失败经 errorOccurred 信号通知 QML；底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
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
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    // 库存页过滤词（v2.14）：非空时列表请求携带 keyword 服务端过滤；由过滤框防抖后写入
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    // 首页加载失败且无任何可显示数据（无内存、无快照）——页面居中离线视图
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
    // 离线视图文案（按失败类型透传：网络层错误 / 服务器繁忙 / 其它服务端文案）
    Q_PROPERTY(QString loadFailedMessage READ loadFailedMessage NOTIFY loadFailedMessageChanged)

public:
    // db：本地快照存储。生产传 nullptr（使用单例 LocalDatabase::instance()）；测试注入独立内存库
    explicit InventoryViewModel(IGoCookApi *api, QObject *parent = nullptr, LocalDatabase *db = nullptr);

    QVariantList items() const;
    bool isLoading() const;
    bool hasMore() const;
    QString filterText() const;
    bool loadFailed() const;
    QString loadFailedMessage() const;
    // 设置过滤词（trim 后生效）：变化时重置到第一页并按新词加载。
    // Q_INVOKABLE：QML 过滤框防抖后直接调用（Q_PROPERTY WRITE 只支持属性赋值，
    // 不会生成 QML 可调用的 setFilterText 函数——曾致 InventoryPage 运行时 TypeError）
    Q_INVOKABLE void setFilterText(const QString& text);

    // 库存操作
    // 加载第一页库存（开启新一轮请求：作废全部在途请求并立即发送，见 m_epoch）
    Q_INVOKABLE void loadInventory(int page = 1, int size = 50);
    // 加载下一页库存
    Q_INVOKABLE void loadNextPage();
    // 添加库存项（expiryDate 为空表示无保质期）
    Q_INVOKABLE void addItem(const QString& name, double quantity, const QString& unit,
                             const QString& expiryDate = "");
    // 删除库存项
    Q_INVOKABLE void deleteItem(int itemId);
    // 更新库存项
    Q_INVOKABLE void updateItem(int itemId, const QString& name, double quantity,
                                const QString& unit, const QString& expiryDate = "");
    // 重新加载当前页
    Q_INVOKABLE void refresh();
    // 清空全部数据与加载状态（登出/账号切换时调用，杜绝上一账号残留数据串台）
    Q_INVOKABLE void clearAll();
    // 网络恢复（main.cpp 接 HttpGoCookApi::networkRestored）：数据可能过期（上次加载失败/无数据）时自动重拉
    void onNetworkRestored();
    // 退避重试间隔（毫秒，默认 5s→10s→20s→30s 封顶；测试注入短间隔用）
    void setRetryDelaysMs(const QList<int>& delays);

signals:
    // 数据变更
    void itemsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    // 过滤词变化（清空/切换账号时 UI 需同步回显）
    void filterTextChanged();
    // 离线视图状态变化（loadFailed 显隐）
    void loadFailedChanged();
    // 离线视图文案变化（失败类型变化 / 退出离线态清空）
    void loadFailedMessageChanged();
    // 操作失败（携带错误描述）
    void errorOccurred(const QString& error);

private:
    // 首页加载失败善后：快照静默兜底（过滤态按词本地过滤/重派生）→ 无快照则置 loadFailed；
    // 瞬时故障（网络层/503）静默并启动退避重试；非瞬时且数据原样保留时发页内提示
    void handleFirstPageFailure(const QString& error);
    // 退出离线态（清 loadFailed + 文案；clearAll / 401 守卫 / 加载成功三处统一收口）
    void clearLoadFailed();
    // 退避重试：排下一档（仅瞬时故障触发——网络层错误 / 服务器繁忙 503；成功/clearAll/登出即停）
    void scheduleRetry();
    void stopRetry();
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
    // 请求代次：每次发起新一轮首屏请求（换词/刷新/翻页轮次重置）时递增；响应带回发送时代次，
    // 到达时代次已落后即属过期（被更新的请求取代），静默丢弃不落数据不改状态
    int m_epoch = 0;
};
