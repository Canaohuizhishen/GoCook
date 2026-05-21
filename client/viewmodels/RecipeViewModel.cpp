#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>
#include <QPointer>
#include <QStringList>

RecipeViewModel::RecipeViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList RecipeViewModel::recipes() const { return m_recipes; }
bool RecipeViewModel::isLoading() const { return m_isLoading; }
bool RecipeViewModel::hasMore() const { return m_hasMore; }
bool RecipeViewModel::healthFilterApplied() const { return m_healthFilterApplied; }
QVariantMap RecipeViewModel::recipeDetail() const { return m_recipeDetail; }
bool RecipeViewModel::detailLoading() const { return m_detailLoading; }
QVariantList RecipeViewModel::searchResults() const { return m_searchResults; }
bool RecipeViewModel::searchLoading() const { return m_searchLoading; }
bool RecipeViewModel::searchHasMore() const { return m_searchHasMore; }
bool RecipeViewModel::searchPerformed() const { return m_searchPerformed; }

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

    if (m_currentMode == LoadMode::Recommended)
        loadRecommendedRecipes(1, m_pageSize);
    else
        loadPublicRecipes(1, m_pageSize);
}

void RecipeViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;

    if (m_currentMode == LoadMode::Recommended)
        loadRecommendedRecipes(m_currentPage + 1, m_pageSize);
    else
        loadPublicRecipes(m_currentPage + 1, m_pageSize);
}

void RecipeViewModel::loadPublicRecipes(int page, int size)
{
    m_currentMode = LoadMode::Public;
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
    m_currentMode = LoadMode::Recommended;
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

void RecipeViewModel::loadRecipeDetail(int recipeId)
{
    m_detailLoading = true;
    m_recipeVideos.clear();
    m_videosLoading = false;
    emit recipeVideosChanged();
    emit videosLoadingChanged();
    emit detailLoadingChanged();

    m_api->getRecipeDetail(recipeId,
                           [self = QPointer<RecipeViewModel>(this)](bool success, const gocook::models::RecipeDetail& data, const std::string& error) {
                               if (!self) return;
                               if (!success) {
                                   emit self->errorOccurred(QString::fromStdString(error));
                                   self->m_detailLoading = false;
                                   emit self->detailLoadingChanged();
                                   return;
                               }

                               self->m_recipeDetail = DataMapper::toMap(data);
                               self->m_detailLoading = false;
                               emit self->detailLoadingChanged();
                               emit self->recipeDetailChanged();
                           });
}

void RecipeViewModel::submitRecipe(const QString& name, const QString& description,
                                    const QString& imageUrl, const QVariantList& ingredients,
                                    const QVariantList& steps, const QVariantList& tags)
{
    gocook::models::SubmitRecipeRequest req;
    req.name = name.toStdString();
    req.description = description.toStdString();
    req.image_url = imageUrl.toStdString();

    for (const auto& v : ingredients) {
        QVariantMap m = v.toMap();
        gocook::models::Ingredient ing;
        ing.name = m["name"].toString().toStdString();
        ing.quantity = m["quantity"].toDouble();
        ing.unit = m["unit"].toString().toStdString();
        req.ingredients.push_back(ing);
    }

    for (const auto& v : steps) {
        QVariantMap m = v.toMap();
        gocook::models::CookingStep step;
        step.order = m["order"].toInt();
        step.description = m["description"].toString().toStdString();
        if (m.contains("duration"))
            step.duration = m["duration"].toInt();
        req.steps.push_back(step);
    }

    for (const auto& v : tags) {
        req.tags.push_back(v.toString().toStdString());
    }

    m_api->submitRecipe(req, [self = QPointer<RecipeViewModel>(this)](bool success,
                                 const gocook::models::SubmitRecipeResponse& data,
                                 const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->submitFailed(QString::fromStdString(error));
            return;
        }
        emit self->recipeSubmitted(data.id, QString::fromStdString(data.status));
    });
}

void RecipeViewModel::searchRecipes(const QString& keyword, int page, int size)
{
    if (keyword.trimmed().isEmpty())
        return;

    m_lastKeyword = keyword;
    m_searchPage = page;
    m_searchLoading = true;
    if (page == 1) {
        m_searchPerformed = false;
    }
    emit searchLoadingChanged();

    m_api->searchRecipes(keyword.toStdString(), page, size, {},
                         [self = QPointer<RecipeViewModel>(this), page]
                         (bool success, const gocook::models::PagedRecipes& data,
                          const std::string& error) {
                             if (!self) return;
                             if (!success) {
                                 emit self->searchErrorOccurred(QString::fromStdString(error));
                                 self->m_searchLoading = false;
                                 emit self->searchLoadingChanged();
                                 return;
                             }

                             if (page == 1) {
                                 self->m_searchResults.clear();
                             }

                             for (const auto& recipe : data.data) {
                                 auto item = DataMapper::toMap(recipe);
                                 self->m_searchResults.append(item);
                             }

                             self->m_searchPage = data.pagination.page;
                             self->m_searchTotalPages = data.pagination.total_pages;
                             self->m_searchHasMore = (self->m_searchPage < self->m_searchTotalPages);

                             emit self->searchResultsChanged();
                             emit self->searchHasMoreChanged();
                             self->m_searchLoading = false;
                             self->m_searchPerformed = true;
                             emit self->searchPerformedChanged();
                             emit self->searchLoadingChanged();
                         });
}

void RecipeViewModel::searchNextPage()
{
    if (m_searchLoading || !m_searchHasMore)
        return;
    searchRecipes(m_lastKeyword, m_searchPage + 1, m_pageSize);
}

void RecipeViewModel::resetSearch()
{
    m_searchResults.clear();
    m_searchHasMore = false;
    m_searchPerformed = false;
    m_searchPage = 1;
    m_searchTotalPages = 0;
    m_lastKeyword.clear();
    emit searchResultsChanged();
    emit searchHasMoreChanged();
    emit searchPerformedChanged();
}

QVariantMap RecipeViewModel::nutritionReport() const { return m_nutritionReport; }
bool RecipeViewModel::nutritionLoading() const { return m_nutritionLoading; }
QVariantList RecipeViewModel::recipeVideos() const { return m_recipeVideos; }
bool RecipeViewModel::videosLoading() const { return m_videosLoading; }
QVariantList RecipeViewModel::myRecipes() const { return m_myRecipes; }
bool RecipeViewModel::myRecipesLoading() const { return m_myRecipesLoading; }
bool RecipeViewModel::myRecipesHasMore() const { return m_myRecipesHasMore; }

void RecipeViewModel::loadNutritionReport(int recipeId)
{
    m_nutritionLoading = true;
    emit nutritionLoadingChanged();

    m_api->getRecipeNutrition(recipeId,
        [self = QPointer<RecipeViewModel>(this)](bool success, const gocook::models::NutritionReport& data, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_nutritionLoading = false;
                emit self->nutritionLoadingChanged();
                return;
            }
            self->m_nutritionReport = DataMapper::toMap(data);
            self->m_nutritionLoading = false;
            emit self->nutritionLoadingChanged();
            emit self->nutritionReportChanged();
        });
}

void RecipeViewModel::loadRecipeVideos(int recipeId)
{
    m_videosLoading = true;
    emit videosLoadingChanged();

    m_api->getRecipeVideos(recipeId,
        [self = QPointer<RecipeViewModel>(this)](bool success, const std::vector<gocook::models::RecipeVideo>& data, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_videosLoading = false;
                emit self->videosLoadingChanged();
                return;
            }
            QVariantList list;
            for (const auto& v : data)
                list.append(DataMapper::toMap(v));
            self->m_recipeVideos = list;
            self->m_videosLoading = false;
            emit self->videosLoadingChanged();
            emit self->recipeVideosChanged();
        });
}

void RecipeViewModel::loadMyRecipes(int page, int size, const QString& status)
{
    if (m_myRecipesLoading) return;

    m_myRecipesPage = page;
    m_myRecipesStatus = status;
    m_myRecipesLoading = true;
    if (page == 1) {
        m_myRecipes.clear();
        emit myRecipesChanged();
    }
    emit myRecipesLoadingChanged();

    m_api->getMySubmittedRecipes(page, size, status.toStdString(),
        [self = QPointer<RecipeViewModel>(this), page]
        (bool success, const gocook::models::PagedMyRecipes& data, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_myRecipesLoading = false;
                emit self->myRecipesLoadingChanged();
                return;
            }

            if (page == 1) {
                self->m_myRecipes.clear();
            }

            for (const auto& item : data.data) {
                auto map = DataMapper::toMap(item);
                self->m_myRecipes.append(map);
            }

            self->m_myRecipesPage = data.pagination.page;
            self->m_myRecipesTotalPages = data.pagination.total_pages;
            self->m_myRecipesHasMore = (self->m_myRecipesPage < self->m_myRecipesTotalPages);

            emit self->myRecipesChanged();
            emit self->myRecipesHasMoreChanged();
            self->m_myRecipesLoading = false;
            emit self->myRecipesLoadingChanged();
        });
}

void RecipeViewModel::loadMyRecipesNextPage()
{
    if (m_myRecipesLoading || !m_myRecipesHasMore) return;
    loadMyRecipes(m_myRecipesPage + 1, 20, m_myRecipesStatus);
}
