#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>
#include <QPointer>

RecipeViewModel::RecipeViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList RecipeViewModel::recipes() const { return m_recipes; }
bool RecipeViewModel::isLoading() const { return m_isLoading; }
bool RecipeViewModel::hasMore() const { return m_hasMore; }
bool RecipeViewModel::healthFilterApplied() const { return m_healthFilterApplied; }

void RecipeViewModel::setHealthFilterApplied(bool applied)
{
    if (m_healthFilterApplied != applied) {
        m_healthFilterApplied = applied;
        emit healthFilterAppliedChanged();
    }
}

void RecipeViewModel::refresh()
{
    m_currentPage = 1;
    m_recipes.clear();
    emit recipesChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadPublicRecipes(1, m_pageSize);
}

void RecipeViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadPublicRecipes(m_currentPage + 1, m_pageSize);
}

void RecipeViewModel::loadPublicRecipes(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getPublicRecipes(page, size, {},
                            [self = QPointer<RecipeViewModel>(this), page](bool success, const gocook::models::PagedRecipes& data, const std::string& error) {
                                if (!self) return;
                                if (!success) {
                                    emit self->errorOccurred(QString::fromStdString(error));
                                    self->m_isLoading = false;
                                    emit self->isLoadingChanged();
                                    return;
                                }

                                if (page == 1) {
                                    self->m_recipes.clear();
                                }

                                for (const auto& recipe : data.data) {
                                    auto item = DataMapper::toMap(recipe);
                                    self->m_recipes.append(item);
                                }

                                self->m_currentPage = data.pagination.page;
                                self->m_totalPages = data.pagination.total_pages;
                                self->m_hasMore = (self->m_currentPage < self->m_totalPages);

                                emit self->recipesChanged();
                                emit self->hasMoreChanged();
                                self->m_isLoading = false;
                                emit self->isLoadingChanged();
                            });
}

void RecipeViewModel::loadRecommendedRecipes(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getRecommendedRecipes(page, size,
                                 [self = QPointer<RecipeViewModel>(this)](bool success, const gocook::models::PagedRecommendedRecipes& data, const std::string& error) {
                                     if (!self) return;
                                     if (!success) {
                                         emit self->errorOccurred(QString::fromStdString(error));
                                         self->m_isLoading = false;
                                         emit self->isLoadingChanged();
                                         return;
                                     }

                                     self->setHealthFilterApplied(data.health_filter_applied);

                                     self->m_recipes.clear();
                                     for (const auto& recipe : data.data) {
                                         auto item = DataMapper::toMap(recipe);
                                         self->m_recipes.append(item);
                                     }

                                     self->m_hasMore = false;
                                     emit self->recipesChanged();
                                     emit self->hasMoreChanged();
                                     self->m_isLoading = false;
                                     emit self->isLoadingChanged();
                                 });
}
