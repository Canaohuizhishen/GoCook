#include "ShoppingListViewModel.h"
#include <DataMapper.h>
#include <QPointer>

ShoppingListViewModel::ShoppingListViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList ShoppingListViewModel::shoppingLists() const { return m_shoppingLists; }
QVariantMap ShoppingListViewModel::currentList() const { return m_currentList; }
bool ShoppingListViewModel::isLoading() const { return m_isLoading; }
bool ShoppingListViewModel::creating() const { return m_creating; }

void ShoppingListViewModel::refresh()
{
    loadShoppingLists();
}

void ShoppingListViewModel::loadShoppingLists()
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getShoppingLists([self = QPointer<ShoppingListViewModel>(this)](bool success,
                                const std::vector<gocook::models::ShoppingListSummary>& data,
                                const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->errorOccurred(QString::fromStdString(error));
            self->m_isLoading = false;
            emit self->isLoadingChanged();
            return;
        }

        self->m_shoppingLists.clear();
        for (const auto& summary : data)
            self->m_shoppingLists.append(DataMapper::toMap(summary));

        emit self->shoppingListsChanged();
        self->m_isLoading = false;
        emit self->isLoadingChanged();
    });
}

void ShoppingListViewModel::loadShoppingListDetail(int listId)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getShoppingListDetail(listId,
        [self = QPointer<ShoppingListViewModel>(this)](bool success,
                              const gocook::models::ShoppingList& data,
                              const std::string& error) {
            if (!self) return;
            self->m_isLoading = false;
            emit self->isLoadingChanged();

            if (success) {
                self->m_currentList = DataMapper::toMap(data);
                emit self->currentListChanged();
                emit self->shoppingListDetailReady();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::batchAddShoppingItems(int listId, const QVariantList& items)
{
    m_isLoading = true;
    emit isLoadingChanged();

    std::vector<gocook::models::BatchShoppingItem> batchItems;
    for (const auto& val : items) {
        QVariantMap map = val.toMap();
        gocook::models::BatchShoppingItem item;
        item.ingredient_name = map["ingredient_name"].toString().toStdString();
        item.quantity = map["quantity"].toDouble();
        item.unit = map["unit"].toString().toStdString();
        batchItems.push_back(std::move(item));
    }

    m_api->batchAddShoppingItems(listId, batchItems,
        [self = QPointer<ShoppingListViewModel>(this), listId](bool success,
                              const gocook::models::BatchShoppingResponse& resp,
                              const std::string& error) {
            if (!self) return;
            self->m_isLoading = false;
            emit self->isLoadingChanged();

            if (success) {
                // 刷新详情以反映最新数据
                self->loadShoppingListDetail(listId);
                emit self->batchAddComplete(QString::fromStdString(resp.message));
            } else {
                emit self->batchAddFailed(QString::fromStdString(error));
            }
        });
}

void ShoppingListViewModel::createShoppingList(const QString& name)
{
    m_creating = true;
    emit creatingChanged();

    gocook::models::CreateShoppingListRequest req;
    req.name = name.toStdString();

    m_api->createShoppingList(req,
        [self = QPointer<ShoppingListViewModel>(this), name](bool success,
                              const gocook::models::ShoppingList& data,
                              const std::string& error) {
            if (!self) return;
            self->m_creating = false;
            emit self->creatingChanged();

            if (success) {
                self->m_currentList = DataMapper::toMap(data);
                emit self->currentListChanged();
                emit self->shoppingListCreated(name);
                self->refresh();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
                emit self->shoppingListCreateFailed(QString::fromStdString(error));
            }
        });
}
