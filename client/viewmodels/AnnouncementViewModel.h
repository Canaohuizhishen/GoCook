#pragma once

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <gocook/IGoCookApi.h>

/**
 * @brief 系统通知（公告）ViewModel：独立列表页数据源 + 已读水位上报（v2.23 与通知中心解耦）。
 *
 * 核心语义（对齐 api-spec 3.13）：
 *   - 「进入系统通知页即已读」：第一页加载成功 → 上报当前最大公告 id（水位，幂等）→ 服务端红点熄灭；
 *   - 公告为全局数据、无逐条已读；「删除」= 会话级本地隐藏（服务端无删除接口，同会话内刷新/翻页
 *     不复活，重启/登出后恢复）；无「全部已读」，点开即看，看完即清；
 *   - 上报失败静默（下一轮汇总自愈），列表加载失败发 errorOccurred（页面可重试）。
 *   - 消息页预览：loadPreview() 静默拉取最新一条公告（标题/时间）供消息页三横条展示；
 *     刻意不触发已读语义（不发 announcementsSeen、不上报水位）——红点只在真正进入本页时熄灭。
 */
class AnnouncementViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList announcements READ announcements NOTIFY announcementsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)
    Q_PROPERTY(QString previewTitle READ previewTitle NOTIFY previewChanged)          ///< 预览：最新公告标题（loadPreview 拉取；失败保留旧值）
    Q_PROPERTY(QString previewCreatedAt READ previewCreatedAt NOTIFY previewChanged)  ///< 预览：最新公告时间（原始串，QML 做相对格式化）

public:
    explicit AnnouncementViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    QVariantList announcements() const;
    bool isLoading() const;
    bool hasMore() const;
    bool isRefreshing() const;
    QString previewTitle() const;
    QString previewCreatedAt() const;

    // 列表操作
    /// 加载第一页（page=1 为「进入系统通知页」：成功发 announcementsSeen 并上报水位）；失败发 errorOccurred。
    Q_INVOKABLE void loadAnnouncements(int page = 1, int size = 20);
    /// 加载下一页（加载中或 !hasMore 时忽略；续页不重报）。
    Q_INVOKABLE void loadNextPage();
    /// 消息页预览拉取：仅取最新一条公告（page1/size1）的标题与时间。
    /// 不触发「进入系统通知页」的已读语义（不发 announcementsSeen、不上报水位、不动列表与红点，也不驱动加载/刷新指示器）；失败静默保留旧值。
    Q_INVOKABLE void loadPreview();
    /// 会话级隐藏公告（页面「删除」按钮的语义）：记入本地隐藏集合并从当前列表移除；
    /// 同会话内刷新/翻页不复活，clearAll（登出/换号）后恢复。服务端无删除接口，不产生网络请求。
    Q_INVOKABLE void hideAnnouncement(int announcementId);
    /// 进入页面 / 下拉刷新：清空并重载第一页（成功同样视为已读）。
    Q_INVOKABLE void refresh();
    /// 清空全部数据与状态（登出 / 401 自动登出 / 注销的统一清理，main.cpp 单点接线调用）。
    void clearAll();

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；结果信号的触发见对应方法注释。
    void announcementsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void isRefreshingChanged();
    /// 列表加载失败（文案可直接展示）。
    void errorOccurred(const QString& error);
    /// 「看过」达成（第一页加载成功即发；main.cpp 接线 → 通知 VM 清系统红点；上报网络结果静默）
    void announcementsSeen();
    /// 预览标题/时间任一变化
    void previewChanged();

private:
    IGoCookApi *m_api;                ///< API 门面（构造注入）
    QVariantList m_announcements;     ///< 已加载公告（page 递增追加）
    bool m_isLoading = false;         ///< 加载请求在途
    bool m_hasMore = false;           ///< 还有下一页
    bool m_isRefreshing = false;      ///< 刷新态（refresh 置真；完成回调置假）
    int m_currentPage = 1;            ///< 已加载到第几页
    int m_totalPages = 0;             ///< 服务端总页数
    int m_reportedWatermark = 0;      ///< 本会话已成功上报的水位（防重复；clearAll 重置）
    QString m_previewTitle;           ///< 预览：最新公告标题（loadPreview 拉取）
    QString m_previewCreatedAt;       ///< 预览：最新公告时间（原始串）
    QSet<int> m_hiddenIds;            ///< 会话级隐藏的公告 id（"删除"按钮；clearAll 清空）
};
