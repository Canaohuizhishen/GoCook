#pragma once

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <gocook/IGoCookApi.h>

/**
 * @brief 购物清单域 ViewModel：管理清单列表、详情、条目更新与导出。
 *
 * 所有 Q_INVOKABLE 均为异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新，
 * 失败经 errorOccurred 信号通知 QML；底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
 */
class ShoppingListViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList shoppingLists READ shoppingLists NOTIFY shoppingListsChanged)
    Q_PROPERTY(QVariantMap currentList READ currentList NOTIFY currentListChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)
    Q_PROPERTY(int deletingListId READ deletingListId NOTIFY deletingListIdChanged)

public:
    explicit ShoppingListViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList shoppingLists() const;
    QVariantMap currentList() const;
    bool isLoading() const { return m_pendingRequests > 0; }
    bool creating() const;
    int deletingListId() const { return m_deletingListId; }

    // 清单操作
    // 加载清单列表
    Q_INVOKABLE void loadShoppingLists();
    // 加载清单详情
    Q_INVOKABLE void loadShoppingListDetail(int listId);
    // 批量添加清单项
    Q_INVOKABLE void batchAddShoppingItems(int listId, const QVariantList& items);
    // 更新清单项勾选状态（快速连点时合并为最后一次状态）
    Q_INVOKABLE void updateShoppingListItem(int listId, int itemId, bool checked);
    // 删除清单（等待接口返回后刷新列表）
    Q_INVOKABLE void deleteShoppingList(int listId);
    // 乐观删除清单：立即从本地列表移除，API 失败时插回
    Q_INVOKABLE void deleteShoppingListOptimistic(int listId, QVariantMap listData);
    // 创建空清单
    Q_INVOKABLE void createShoppingList(const QString& name);
    // 根据菜谱缺失食材创建清单（创建成功后批量添加条目）
    Q_INVOKABLE void createListFromRecipe(const QString& name, const QVariantList& missingIngredients);
    // 重新加载清单列表
    Q_INVOKABLE void refresh();
    // 导出清单（当前实现固定导出为纯文本）
    Q_INVOKABLE void exportShoppingList(int listId);

signals:
    // 数据变更
    void shoppingListsChanged();
    void currentListChanged();
    void isLoadingChanged();
    void creatingChanged();
    // 操作结果
    void errorOccurred(const QString& error);
    void shoppingListCreated(const QString& name);
    void shoppingListCreateFailed(const QString& error);
    void shoppingListDetailReady();
    void batchAddComplete(const QString& message);
    void batchAddFailed(const QString& error);
    void itemUpdated();
    void shoppingListDeleted(int listId);
    void deletingListIdChanged();
    void exportReady(const QString& content);

private:
    // 请求计数 +1（isLoading 据此翻转）
    void beginLoad();
    // 请求计数 -1（isLoading 据此翻转）
    void endLoad();

    IGoCookApi *m_api;
    QVariantList m_shoppingLists;
    QVariantMap m_currentList;
    int m_pendingRequests = 0;
    bool m_creating = false;
    int m_deletingListId = -1;    // 正在删除的 listId（-1 = 无）
    QSet<int> m_pendingDeleteIds; // 乐观删除中但 API 尚未返回的 listId

    // 快速连续点击时：只记最后一次状态，避免静默丢弃或并发覆盖
    int m_updatePendingItemId = -1;   // 等待中的 itemId（-1 = 无）
    bool m_updatePendingChecked = false;
    bool m_updateInFlight = false;    // 是否正在发送更新请求
};
