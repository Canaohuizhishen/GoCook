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

void RecipeViewModel::loadFavorites(int page, int size, const QString &group)
{
    if (m_favoritesLoading) return;
    m_favoritesLoading = true;
    emit favoritesLoadingChanged();

    m_api->getFavorites(page, size, group.toStdString(),
                        [self = QPointer<RecipeViewModel>(this), page]
                        (bool success, const gocook::models::PagedFavorites& data,
                         const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->errorOccurred(QString::fromStdString(error));
            self->m_favoritesLoading = false;
            emit self->favoritesLoadingChanged();
            return;
        }

        if (page == 1) {
            self->m_favorites.clear();
        }

        for (const auto& fav : data.data) {
            auto item = DataMapper::toMap(fav);
            self->m_favorites.append(item);
        }

        self->m_favoritesPage = data.pagination.page;
        self->m_favoritesTotalPages = data.pagination.total_pages;
        self->m_favoritesTotal = data.pagination.total;
        self->m_favoritesHasMore = (self->m_favoritesPage < self->m_favoritesTotalPages);

        emit self->favoritesChanged();
        emit self->favoritesHasMoreChanged();
        self->m_favoritesLoading = false;
        emit self->favoritesLoadingChanged();
    });
}

void RecipeViewModel::loadMoreFavorites()
{
    if (m_favoritesLoading || !m_favoritesHasMore) return;
    loadFavorites(m_favoritesPage + 1, m_pageSize);
}

void RecipeViewModel::toggleFavorite(int recipeId, int groupId)
{
    std::optional<int> optGroupId = (groupId > 0) ? std::optional<int>(groupId) : std::nullopt;
    m_api->toggleFavorite(recipeId, optGroupId, std::nullopt,
        [self = QPointer<RecipeViewModel>(this), recipeId]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->favoriteToggleSuccess(recipeId, true);
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "操作失败" : error));
        }
    });
}

void RecipeViewModel::loadFavoriteGroups()
{
    m_api->getFavoriteGroups([self = QPointer<RecipeViewModel>(this)]
                             (bool success,
                              const std::vector<gocook::models::FavoriteGroup>& groups,
                              const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->favoriteOperationFailed(QString::fromStdString(error));
            return;
        }
        QVariantList list;
        for (const auto& g : groups) {
            QVariantMap item;
            item["id"] = g.id;
            item["name"] = QString::fromStdString(g.name);
            item["sort_order"] = g.sort_order;
            item["count"] = g.count;
            list.append(item);
        }
        self->m_favoriteGroups = list;
        emit self->favoriteGroupsChanged();
    });
}

void RecipeViewModel::createFavoriteGroup(const QString &name)
{
    gocook::models::CreateGroupRequest req;
    req.name = name.toStdString();
    m_api->createFavoriteGroup(req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const gocook::models::FavoriteGroup&, const std::string& error) {
        if (!self) return;
        if (success) {
            self->loadFavoriteGroups();
            emit self->favoriteGroupCreated();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "创建失败" : error));
        }
    });
}

void RecipeViewModel::deleteFavoriteGroup(int groupId)
{
    m_api->deleteFavoriteGroup(groupId,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->loadFavoriteGroups();
            self->loadFavorites(1, 20);
            emit self->favoriteGroupDeleted();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "删除失败" : error));
        }
    });
}

void RecipeViewModel::removeFavorite(int favoriteId)
{
    gocook::models::BatchDeleteFavoritesRequest req;
    req.favorite_ids = {favoriteId};
    m_api->batchDeleteFavorites(req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->m_favoritesLoading = false;
            self->loadFavorites(1, 20);
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "取消收藏失败" : error));
        }
    });
}

void RecipeViewModel::batchRemoveFavorites(const QVariantList &favoriteIds)
{
    gocook::models::BatchDeleteFavoritesRequest req;
    for (const auto &v : favoriteIds)
        req.favorite_ids.push_back(v.toInt());
    if (req.favorite_ids.empty()) return;
    m_api->batchDeleteFavorites(req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->m_favoritesLoading = false;
            self->loadFavorites(1, 20);
            self->loadFavoriteGroups();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "批量删除失败" : error));
        }
    });
}

void RecipeViewModel::moveFavorite(int favoriteId, int groupId)
{
    gocook::models::UpdateFavoriteRequest req;
    req.group_id = groupId;
    m_api->updateFavoriteItem(favoriteId, req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->favoriteMoved();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "移动失败" : error));
        }
    });
}

void RecipeViewModel::batchMoveFavorites(const QVariantList &favoriteIds, int groupId)
{
    if (favoriteIds.isEmpty()) return;
    gocook::models::UpdateFavoriteRequest req;
    req.group_id = groupId;
    auto self = QPointer<RecipeViewModel>(this);
    std::shared_ptr<int> pending = std::make_shared<int>(favoriteIds.size());
    for (int i = 0; i < favoriteIds.size(); ++i) {
        m_api->updateFavoriteItem(favoriteIds[i].toInt(), req,
            [self, pending](bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->favoriteOperationFailed(QString::fromStdString(
                    error.empty() ? "批量移动失败" : error));
            }
            if (--(*pending) == 0) {
                emit self->favoriteMoved();
            }
        });
    }
}

void RecipeViewModel::updateFavoriteGroupName(int groupId, const QString &name)
{
    gocook::models::UpdateGroupRequest req;
    req.name = name.toStdString();
    m_api->updateFavoriteGroup(groupId, req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            self->loadFavoriteGroups();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "更新失败" : error));
        }
    });
}
