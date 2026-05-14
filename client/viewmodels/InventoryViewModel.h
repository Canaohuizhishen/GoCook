#pragma once

#include <QObject>
#include <QVariantList>
#include <gocook/IGoCookApi.h>

class InventoryViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(int deletingId READ deletingId NOTIFY deletingIdChanged)

public:
    explicit InventoryViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList items() const;
    bool isLoading() const;
    bool hasMore() const;
    int deletingId() const;

    Q_INVOKABLE void loadInventory(int page = 1, int size = 50);
    Q_INVOKABLE void loadNextPage();
    Q_INVOKABLE void addItem(const QString& name, double quantity, const QString& unit,
                             const QString& expiryDate = "");
    Q_INVOKABLE void deleteItem(int itemId);
    Q_INVOKABLE void updateItem(int itemId, const QString& name, double quantity,
                                const QString& unit, const QString& expiryDate = "");
    Q_INVOKABLE void refresh();

signals:
    void itemsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void deletingIdChanged();
    void errorOccurred(const QString& error);

private:
    IGoCookApi *m_api;
    QVariantList m_items;
    bool m_isLoading = false;
    bool m_hasMore = false;
    int m_currentPage = 1;
    int m_pageSize = 50;
    int m_totalPages = 0;
    int m_deletingId = -1;
};
