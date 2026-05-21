#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <gocook/IGoCookApi.h>

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

    Q_INVOKABLE void loadShoppingLists();
    Q_INVOKABLE void loadShoppingListDetail(int listId);
    Q_INVOKABLE void batchAddShoppingItems(int listId, const QVariantList& items);
    Q_INVOKABLE void updateShoppingListItem(int listId, int itemId, bool checked);
    Q_INVOKABLE void deleteShoppingList(int listId);
    Q_INVOKABLE void createShoppingList(const QString& name);
    Q_INVOKABLE void refresh();

signals:
    void shoppingListsChanged();
    void currentListChanged();
    void isLoadingChanged();
    void creatingChanged();
    void errorOccurred(const QString& error);
    void shoppingListCreated(const QString& name);
    void shoppingListCreateFailed(const QString& error);
    void shoppingListDetailReady();
    void batchAddComplete(const QString& message);
    void batchAddFailed(const QString& error);
    void itemUpdated();
    void shoppingListDeleted(int listId);
    void deletingListIdChanged();

private:
    void beginLoad();
    void endLoad();

    IGoCookApi *m_api;
    QVariantList m_shoppingLists;
    QVariantMap m_currentList;
    int m_pendingRequests = 0;
    bool m_creating = false;
    int m_deletingListId = -1;    // 正在删除的 listId（-1 = 无）

    // 快速连续点击时：只记最后一次状态，避免静默丢弃或并发覆盖
    int m_updatePendingItemId = -1;   // 等待中的 itemId（-1 = 无）
    bool m_updatePendingChecked = false;
    bool m_updateInFlight = false;    // 是否正在发送更新请求
};
