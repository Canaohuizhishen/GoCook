#include "InventoryViewModel.h"
#include <DataMapper.h>
#include <QPointer>

InventoryViewModel::InventoryViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList InventoryViewModel::items() const { return m_items; }
bool InventoryViewModel::isLoading() const { return m_isLoading; }
bool InventoryViewModel::hasMore() const { return m_hasMore; }
int InventoryViewModel::deletingId() const { return m_deletingId; }

void InventoryViewModel::refresh()
{
    m_currentPage = 1;
    m_items.clear();
    emit itemsChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadInventory(m_currentPage + 1, m_pageSize);
}

void InventoryViewModel::loadInventory(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getInventory(page, size,
                        [self = QPointer<InventoryViewModel>(this), page](bool success,
                              const gocook::models::PagedInventory& data,
                              const std::string& error) {
                            if (!self) return;
                            if (!success) {
                                emit self->errorOccurred(QString::fromStdString(error));
                                self->m_isLoading = false;
                                emit self->isLoadingChanged();
                                return;
                            }

                            if (page == 1)
                                self->m_items.clear();

                            for (const auto& item : data.data)
                                self->m_items.append(DataMapper::toMap(item));

                            self->m_currentPage = data.pagination.page;
                            self->m_totalPages = data.pagination.total_pages;
                            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

                            emit self->itemsChanged();
                            emit self->hasMoreChanged();
                            self->m_isLoading = false;
                            emit self->isLoadingChanged();
                        });
}

void InventoryViewModel::addItem(const QString& name, double quantity,
                                  const QString& unit, const QString& expiryDate)
{
    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = name.toStdString();
    req.quantity = quantity;
    req.unit = unit.toStdString();
    if (!expiryDate.isEmpty())
        req.expiry_date = expiryDate.toStdString();

    m_api->upsertInventory(req, [self = QPointer<InventoryViewModel>(this)](bool success, int id, const std::string& error) {
        if (!self) return;
        if (success) {
            self->refresh();
        } else {
            emit self->errorOccurred(QString::fromStdString(error));
        }
    });
}

void InventoryViewModel::deleteItem(int itemId)
{
    m_deletingId = itemId;
    emit deletingIdChanged();

    m_api->deleteInventoryItem(itemId,
                               [self = QPointer<InventoryViewModel>(this), itemId](bool success, const std::string& error) {
                                   if (!self) return;
                                   self->m_deletingId = -1;
                                   emit self->deletingIdChanged();
                                   if (success) {
                                       for (int i = 0; i < self->m_items.size(); ++i) {
                                           if (self->m_items[i].toMap()["id"].toInt() == itemId) {
                                               self->m_items.removeAt(i);
                                               emit self->itemsChanged();
                                               break;
                                           }
                                       }
                                   } else {
                                       emit self->errorOccurred(QString::fromStdString(error));
                                   }
                               });
}
