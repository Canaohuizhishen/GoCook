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
