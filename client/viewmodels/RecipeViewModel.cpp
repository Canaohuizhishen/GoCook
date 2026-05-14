#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>
#include <QPointer>

RecipeViewModel::RecipeViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList RecipeViewModel::recipes() const { return m_recipes; }
bool RecipeViewModel::isLoading() const { return m_isLoading; }
bool RecipeViewModel::healthFilterApplied() const { return m_healthFilterApplied; }

void RecipeViewModel::setHealthFilterApplied(bool applied)
{
    if (m_healthFilterApplied != applied) {
        m_healthFilterApplied = applied;
        emit healthFilterAppliedChanged();
    }
}

void RecipeViewModel::loadPublicRecipes(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getPublicRecipes(page, size, {},
                            [self = QPointer<RecipeViewModel>(this)](bool success, const gocook::models::PagedRecipes& data, const std::string& error) {
                                if (!self) return;
                                if (!success) {
                                    emit self->errorOccurred(QString::fromStdString(error));
                                    self->m_isLoading = false;
                                    emit self->isLoadingChanged();
                                    return;
                                }

                                self->m_recipes.clear();
                                for (const auto& recipe : data.data) {
                                    auto item = DataMapper::toMap(recipe);
                                    self->m_recipes.append(item);
                                }

                                emit self->recipesChanged();
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

                                     emit self->recipesChanged();
                                     self->m_isLoading = false;
                                     emit self->isLoadingChanged();
                                 });
}