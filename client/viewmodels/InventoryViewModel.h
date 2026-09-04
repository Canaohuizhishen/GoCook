#pragma once

#include <QObject>
#include <QVariantList>
#include <gocook/IGoCookApi.h>

/**
 * @brief 库存域 ViewModel：管理库存列表的加载、翻页与增删改。
 *
 * 所有 Q_INVOKABLE 均为异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新，
 * 失败经 errorOccurred 信号通知 QML；底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
 */
class InventoryViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    // 库存页过滤词（v2.14）：非空时列表请求携带 keyword 服务端过滤；由过滤框防抖后写入
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit InventoryViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList items() const;
    bool isLoading() const;
    bool hasMore() const;
    QString filterText() const;
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

signals:
    // 数据变更
    void itemsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    // 过滤词变化（清空/切换账号时 UI 需同步回显）
    void filterTextChanged();
    // 操作失败（携带错误描述）
    void errorOccurred(const QString& error);

private:
    IGoCookApi *m_api;         ///< API 门面（经 HTTP 实现）
    QVariantList m_items;      ///< 当前页库存数据
    QString m_filterText;      ///< 过滤词（空 = 不过滤）
    bool m_isLoading = false;  ///< 是否正在加载
    bool m_hasMore = false;    ///< 是否还有下一页
    int m_currentPage = 1;     ///< 当前页码
    int m_pageSize = 50;       ///< 每页数量
    int m_totalPages = 0;      ///< 总页数
    // 请求代次：每次发起新一轮首屏请求（换词/刷新/翻页轮次重置）时递增；响应带回发送时代次，
    // 到达时代次已落后即属过期（被更新的请求取代），静默丢弃不落数据不改状态
    int m_epoch = 0;
};
