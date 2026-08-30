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

public:
    explicit InventoryViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList items() const;
    bool isLoading() const;
    bool hasMore() const;

    // 库存操作
    // 加载第一页库存
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
    // 操作失败（携带错误描述）
    void errorOccurred(const QString& error);

private:
    IGoCookApi *m_api;         ///< API 门面（经 HTTP 实现）
    QVariantList m_items;      ///< 当前页库存数据
    bool m_isLoading = false;  ///< 是否正在加载
    bool m_hasMore = false;    ///< 是否还有下一页
    int m_currentPage = 1;     ///< 当前页码
    int m_pageSize = 50;       ///< 每页数量
    int m_totalPages = 0;      ///< 总页数
};
