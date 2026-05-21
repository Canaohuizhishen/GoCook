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

public:
    explicit ShoppingListViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList shoppingLists() const;
    QVariantMap currentList() const;
    bool isLoading() const;
    bool creating() const;

    Q_INVOKABLE void loadShoppingLists();
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

private:
    IGoCookApi *m_api;
    QVariantList m_shoppingLists;
    QVariantMap m_currentList;
    bool m_isLoading = false;
    bool m_creating = false;
};
