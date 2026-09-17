#pragma once

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <gocook/IGoCookApi.h>

/**
 * @brief 购物清单域 ViewModel：清单列表、详情、条目勾选与导出。
 *
 * 所有 Q_INVOKABLE 异步：结果经 Q_PROPERTY + NOTIFY 驱动 QML；失败多发 errorOccurred；底层 IGoCookApi。
 *
 * 三条贯穿全类的设计（具体契约见各方法注释）：
 *   1. isLoading 为在途请求计数派生（beginLoad/endLoad 包裹每个请求），并发请求安全。
 *   2. 删除两个变体：deleteShoppingList 等服务器确认（行级 deletingListId 标记）；
 *      deleteShoppingListOptimistic 先本地移除、失败插回原位置。
 *   3. 条目勾选连点合并：在途时只记最后一次状态，当前请求返回后自动补发。
 */
class ShoppingListViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList shoppingLists READ shoppingLists NOTIFY shoppingListsChanged)
    Q_PROPERTY(QVariantMap currentList READ currentList NOTIFY currentListChanged)   ///< 清单详情（加载详情 / 建单成功时写入）
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)   ///< 有请求在途（在途请求计数 > 0 派生）
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)   ///< 建清单请求在途
    Q_PROPERTY(int deletingListId READ deletingListId NOTIFY deletingListIdChanged)   ///< 正在删除的清单 id（-1=无；供行级删除中状态）

public:
    explicit ShoppingListViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QVariantList shoppingLists() const;
    QVariantMap currentList() const;
    bool isLoading() const { return m_pendingRequests > 0; }   ///< 特殊实现：在途请求计数派生
    bool creating() const;
    int deletingListId() const { return m_deletingListId; }

    // 清单操作
    /// 加载清单列表（替换本地缓存；跳过乐观删除中的条目）。失败发 errorOccurred。
    Q_INVOKABLE void loadShoppingLists();
    /// 加载清单详情；成功发 shoppingListDetailReady，失败发 errorOccurred。
    Q_INVOKABLE void loadShoppingListDetail(int listId);
    /// 批量添加条目（items: [{ingredient_name, quantity, unit}]）。
    /// 成功重载详情并发 batchAddComplete，失败发 batchAddFailed。
    Q_INVOKABLE void batchAddShoppingItems(int listId, const QVariantList& items);
    /// 更新条目勾选（连点合并：在途时只记最后一次，返回后自动补发）。
    /// 成功发 itemUpdated，失败发 errorOccurred。
    Q_INVOKABLE void updateShoppingListItem(int listId, int itemId, bool checked);
    /// 删除清单：行级 deletingListId 标记；等接口返回后从列表移除并发 shoppingListDeleted。
    Q_INVOKABLE void deleteShoppingList(int listId);
    /// 乐观删除：立即移除本地条目；失败按原位置插回并发 errorOccurred，成功发 shoppingListDeleted。
    Q_INVOKABLE void deleteShoppingListOptimistic(int listId, QVariantMap listData);
    /// 建空清单：成功写 currentList、发 shoppingListCreated(name) 并刷新列表；
    /// 失败发 errorOccurred 与 shoppingListCreateFailed。
    Q_INVOKABLE void createShoppingList(const QString& name);
    /// 由菜谱缺失食材建清单（items: [{name, quantity, unit}]）：成功后先批量添加再发
    /// shoppingListCreated(name)；失败发 shoppingListCreateFailed / batchAddFailed。
    Q_INVOKABLE void createListFromRecipe(const QString& name, const QVariantList& missingIngredients);
    /// 重新加载清单列表（等价 loadShoppingLists）。
    Q_INVOKABLE void refresh();
    /// 清空全部数据与状态（登出 / 401 自动登出 / 注销的统一清理，main.cpp 单点接线调用）。
    /// 注意：不动 m_pendingRequests——在途请求回调仍需 endLoad 配对计数，不能被清。
    void clearAll();
    /// 导出清单（当前固定纯文本）；成功发 exportReady(content)，失败发 errorOccurred。
    Q_INVOKABLE void exportShoppingList(int listId);

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；结果信号的触发见对应方法注释。
    void shoppingListsChanged();
    void currentListChanged();
    void isLoadingChanged();
    void creatingChanged();
    /// 通用失败通道（列表/详情/删除/导出等；文案可直接展示）。
    void errorOccurred(const QString& error);
    void shoppingListCreated(const QString& name);
    void shoppingListCreateFailed(const QString& error);
    void shoppingListDetailReady();
    void batchAddComplete(const QString& message);
    void batchAddFailed(const QString& error);
    void itemUpdated();
    void shoppingListDeleted(int listId);
    void deletingListIdChanged();
    /// content=导出内容（当前固定纯文本格式）。
    void exportReady(const QString& content);

private:
    /// 请求计数 +1（isLoading 据此翻转）
    void beginLoad();
    /// 请求计数 -1（isLoading 据此翻转）
    void endLoad();

    IGoCookApi *m_api;   ///< API 门面（构造注入）
    QVariantList m_shoppingLists;   ///< 清单列表数据
    QVariantMap m_currentList;   ///< 清单详情数据
    int m_pendingRequests = 0;   ///< 在途请求计数（isLoading 据此翻转）
    bool m_creating = false;   ///< 建单请求在途
    int m_deletingListId = -1;    ///< 正在删除的 listId（-1 = 无）
    QSet<int> m_pendingDeleteIds; ///< 乐观删除中但 API 尚未返回的 listId

    /// 快速连续点击时：只记最后一次状态，避免静默丢弃或并发覆盖
    int m_updatePendingItemId = -1;   ///< 等待中的 itemId（-1 = 无）
    bool m_updatePendingChecked = false;   ///< 等待中的勾选状态
    bool m_updateInFlight = false;    ///< 是否正在发送更新请求
};
