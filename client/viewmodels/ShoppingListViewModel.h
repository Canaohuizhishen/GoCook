#pragma once

#include <QHash>
#include <QList>
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
 *   2. 删除清单：deleteShoppingListOptimistic 先本地移除、失败插回原位置（唯一的删除路径）。
 *   3. 条目勾选连点合并：点击立即乐观落本地；按条目合并末态（同条目覆盖、不同条目各自入队），
 *      FIFO 串行逐条发送；服务端确认后落定，失败回滚到已确认态（契约详见 updateShoppingListItem）。
 */
class ShoppingListViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList shoppingLists READ shoppingLists NOTIFY shoppingListsChanged)
    Q_PROPERTY(QVariantMap currentList READ currentList NOTIFY currentListChanged)   ///< 清单详情（加载详情 / 建单成功时写入）
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)   ///< 有请求在途（在途请求计数 > 0 派生）
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)   ///< 建清单请求在途

public:
    explicit ShoppingListViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QVariantList shoppingLists() const;
    QVariantMap currentList() const;
    bool isLoading() const { return m_pendingRequests > 0; }   ///< 特殊实现：在途请求计数派生
    bool creating() const;

    // 清单操作
    /// 加载清单列表（替换本地缓存；跳过乐观删除中的条目）。失败发 errorOccurred。
    Q_INVOKABLE void loadShoppingLists();
    /// 加载清单详情；成功发 shoppingListDetailReady，失败发 errorOccurred。
    Q_INVOKABLE void loadShoppingListDetail(int listId);
    /// 批量添加条目（items: [{ingredient_name, quantity, unit}]）。
    /// 成功重载详情并发 batchAddComplete，失败发 batchAddFailed。
    Q_INVOKABLE void batchAddShoppingItems(int listId, const QVariantList& items);
    /// 更新条目勾选（连点合并 + 串行队列）：
    ///   - 点击立即乐观更新本地 checked（UI 即时响应，同条目连点也保留正确意图）；
    ///   - 按条目合并末态：同一 item 连续点击只保留最后一个状态，不同 item 各自入队（FIFO）；
    ///   - 单请求在途、逐条串行发送：当前请求返回后自动补发下一个；
    ///   - 成功：该 item 无更新待发时本地落定并发 itemUpdated；失败：发 errorOccurred 并回滚该 item。
    Q_INVOKABLE void updateShoppingListItem(int listId, int itemId, bool checked);
    /// 删除条目（v2.18）：成功从详情本地移除该条目并发 itemUpdated，失败发 errorOccurred。
    /// 已勾选条目的库存回退由服务端在同一事务内完成（删除 = 取消勾选 + 删行）。
    /// 删除时同步剔除该条目的待发勾选（防迟到 PATCH 打向已删行）。
    Q_INVOKABLE void deleteShoppingListItem(int listId, int itemId);
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
    /// content=导出内容（当前固定纯文本格式）。
    void exportReady(const QString& content);

private:
    /// 请求计数 +1（isLoading 据此翻转）
    void beginLoad();
    /// 请求计数 -1（isLoading 据此翻转）
    void endLoad();

    /// 连点入口：乐观落本地 + 按条目合并入队 + 尝试串行发送
    void enqueueItemUpdate(int listId, int itemId, bool checked);
    /// 串行发送器：无在途且队列非空时取队首发出（回调链负责继续驱动）
    void drainItemUpdates();
    /// 把 currentList 中某条目的 checked 写为期望值（条目不存在/无变化则 no-op）
    void applyLocalChecked(int itemId, bool checked);
    /// 以 currentList 全量重建已确认基线，并把在途/待发乐观值回贴（防整表替换覆盖在途点击）
    void rebuildConfirmedAndReapplyOptimistic();

    IGoCookApi *m_api;   ///< API 门面（构造注入）
    QVariantList m_shoppingLists;   ///< 清单列表数据
    QVariantMap m_currentList;   ///< 清单详情数据
    int m_pendingRequests = 0;   ///< 在途请求计数（isLoading 据此翻转）
    bool m_creating = false;   ///< 建单请求在途
    QSet<int> m_pendingDeleteIds; ///< 乐观删除中但 API 尚未返回的 listId

    /// 勾选连点合并 + 串行队列（契约详见 updateShoppingListItem 注释）
    QHash<int, bool> m_pendingChecks;      ///< 待发送：itemId → 期望状态（同条目末态覆盖）
    QHash<int, int> m_pendingItemListIds;  ///< 待发送：itemId → 所属 listId（发送时取用，防跨清单误发）
    QList<int> m_pendingQueue;             ///< 待发送条目 FIFO 顺序（与 m_pendingChecks 同步增删）
    int m_inFlightItemId = -1;             ///< 在途更新请求的条目 id（-1 = 无；串行至多一个）
    bool m_inFlightChecked = false;        ///< 在途请求的期望状态（详情重载回贴显示用）
    QHash<int, bool> m_confirmedChecks;    ///< itemId → 服务端已确认状态（失败回滚基线；整表替换时重建）
    QSet<int> m_deletingItemIds;      ///< 删除请求在途的条目 id（重复点击去重）
};
