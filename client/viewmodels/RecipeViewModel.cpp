#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>
#include <QPointer>
#include <QStringList>
#include <QRandomGenerator>
#include "../api/HttpGoCookApi.h"
#include "../database/LocalDatabase.h"

RecipeViewModel::RecipeViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QString RecipeViewModel::apiBaseUrl() const {
    auto* httpApi = dynamic_cast<const HttpGoCookApi*>(m_api);
    if (httpApi) return httpApi->baseUrl();
    return QStringLiteral("http://127.0.0.1:8080");
}

QVariantList RecipeViewModel::recipes() const { return m_recipes; }
bool RecipeViewModel::isLoading() const { return m_isLoading; }
bool RecipeViewModel::hasMore() const { return m_hasMore; }
bool RecipeViewModel::healthFilterApplied() const { return m_healthFilterApplied; }
QVariantMap RecipeViewModel::recipeDetail() const { return m_recipeDetail; }
bool RecipeViewModel::detailLoading() const { return m_detailLoading; }
bool RecipeViewModel::detailLoadFailed() const { return m_detailLoadFailed; }
bool RecipeViewModel::favoritesLoadFailed() const { return m_favoritesLoadFailed; }
QVariantList RecipeViewModel::searchResults() const { return m_searchResults; }
bool RecipeViewModel::searchLoading() const { return m_searchLoading; }
bool RecipeViewModel::searchHasMore() const { return m_searchHasMore; }
bool RecipeViewModel::searchPerformed() const { return m_searchPerformed; }

// 「全部」总数：Σ 各分组 count。服务端分组列表含合成的"默认收藏夹"组（其 count 即未分组收藏数），
// 故求和恒等于全量收藏数；纯派生计算，不受收藏列表的筛选查询覆盖
int RecipeViewModel::favoritesAllCount() const
{
    int total = 0;
    for (const QVariant& group : m_favoriteGroups)
        total += group.toMap().value("count").toInt();
    return total;
}

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
        shuffleRecommended();   // 推荐语境下「刷新」= 换一批（新 seed）
    else
        loadPublicRecipes(1, m_pageSize);
}

void RecipeViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;

    // 推荐为单批语义（整批替换、无续页）：显式空操作。
    // 若无此拦截，"推荐模式 + 残留 hasMore"的边缘态会把公开列表整列替换为推荐批
    if (m_currentMode == LoadMode::Recommended) return;

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

void RecipeViewModel::loadRecommendedRecipes(int page, int size, unsigned int seed)
{
    m_currentMode = LoadMode::Recommended;
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getRecommendedRecipes(page, size, seed,
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

void RecipeViewModel::shuffleRecommended()
{
    // 「换一批」：生成非零随机种子（0 保留给"确定性首屏"；同 seed 同数据由服务端复现）
    unsigned int seed = QRandomGenerator::global()->generate();
    if (seed == 0) seed = 1;
    loadRecommendedRecipes(1, kRecPageSize, seed);
}

void RecipeViewModel::loadRecipeDetail(int recipeId)
{
    // 记录当前请求的菜谱 id：A→B 快速切换时丢弃 A 的迟到响应，防止覆盖 B 的内容（竞态防护）
    m_detailRequestedId = recipeId;
    // 进入即清空：切换菜谱时不短暂显示上一个菜谱（断网场景尤为重要）
    m_recipeDetail = QVariantMap();
    m_recipeVideos.clear();
    m_videosLoading = false;
    m_detailLoadFailed = false;
    emit recipeDetailChanged();
    emit recipeVideosChanged();
    emit videosLoadingChanged();
    emit detailLoadFailedChanged();
    m_detailLoading = true;
    emit detailLoadingChanged();

    m_api->getRecipeDetail(recipeId,
                           [self = QPointer<RecipeViewModel>(this), recipeId](bool success, const gocook::models::RecipeDetail& data, const std::string& error) {
                               if (!self) return;
                               // 过期响应丢弃：期间用户已切换到其他菜谱（loading 状态由新请求自己管理）
                               if (recipeId != self->m_detailRequestedId) return;
                               if (!success) {
                                   // PDD 式兜底：缓存命中 → 静默显示缓存（页面与联网状态无异，零提示）；
                                   // 无缓存 → 置 detailLoadFailed，页面居中显示「无网络连接」离线视图
                                   const QVariantMap cached =
                                       LocalDatabase::instance()->getRecipeDetailCache(recipeId);
                                   if (!cached.isEmpty()) {
                                       self->m_recipeDetail = cached;
                                       emit self->recipeDetailChanged();
                                   } else {
                                       self->m_recipeDetail = QVariantMap();
                                       self->m_detailLoadFailed = true;
                                       emit self->recipeDetailChanged();
                                       emit self->detailLoadFailedChanged();
                                   }
                                   self->m_detailLoading = false;
                                   emit self->detailLoadingChanged();
                                   return;
                               }

                               self->m_recipeDetail = DataMapper::toMap(data);
                               // 写入本地缓存，断网时详情页兜底显示
                               LocalDatabase::instance()->saveRecipeDetailCache(recipeId, self->m_recipeDetail);
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

void RecipeViewModel::editRecipe(int recipeId, const QString& name, const QString& description,
                                  const QString& imageUrl, const QVariantList& ingredients,
                                  const QVariantList& steps, const QVariantList& tags)
{
    gocook::models::EditRecipeRequest req;
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
        if (m.contains("image_url"))
            step.image_url = m["image_url"].toString().toStdString();
        req.steps.push_back(step);
    }

    for (const auto& v : tags) {
        req.tags.push_back(v.toString().toStdString());
    }

    m_api->editRecipe(recipeId, req, [self = QPointer<RecipeViewModel>(this)](bool success,
                                 const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->editFailed(QString::fromStdString(error));
            return;
        }
        emit self->recipeEdited();
    });
}

void RecipeViewModel::loadRecipeForEdit(int recipeId)
{
    // 先清空旧菜谱详情，避免编辑页（营养合计区等）短暂显示上一菜谱的数据
    m_recipeDetail = QVariantMap();
    emit recipeDetailChanged();

    m_detailLoading = true;
    emit detailLoadingChanged();

    m_api->getRecipeDetail(recipeId, [self = QPointer<RecipeViewModel>(this)](bool success,
                                 const gocook::models::RecipeDetail& data,
                                 const std::string& error) {
        if (!self) return;
        self->m_detailLoading = false;
        emit self->detailLoadingChanged();

        if (!success) {
            emit self->errorOccurred(QString::fromStdString(error));
            return;
        }

        self->m_recipeDetail = DataMapper::toMap(data);
        emit self->recipeDetailChanged();
        emit self->editFormDataReady();
    });
}

void RecipeViewModel::searchRecipes(const QString& keyword, int page, int size)
{
    if (keyword.trimmed().isEmpty())
        return;

    m_lastKeyword = keyword;
    m_searchPage = page;
    m_searchPageSize = size;   // 记录页大小：searchNextPage 续页沿用（服务端 offset=(page-1)*size）
    // 轮次票（RequestEpoch）：首屏（page==1）开启新一轮并作废在途旧词响应（最新词立即发出不被押后）；
    // 续页沿用当前轮次票据——续页在途期间换词，响应到达时票据已失效，同样被丢弃
    const int ticket = (page == 1) ? m_searchEpoch.begin() : m_searchEpoch.current();
    m_searchLoading = true;
    if (page == 1) {
        m_searchPerformed = false;
    }
    emit searchLoadingChanged();

    m_api->searchRecipes(keyword.toStdString(), page, size, {},
                         [self = QPointer<RecipeViewModel>(this), page, ticket]
                         (bool success, const gocook::models::PagedRecipes& data,
                          const std::string& error) {
                             if (!self) return;
                             // 票据失效：本响应属已作废的旧轮次（换词/清空后），静默丢弃——
                             // 不落数据、不改状态（加载标记由最新轮次请求自行收尾）
                             if (!self->m_searchEpoch.isCurrent(ticket)) return;
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
    searchRecipes(m_lastKeyword, m_searchPage + 1, m_searchPageSize);
}

void RecipeViewModel::resetSearch()
{
    m_searchEpoch.invalidate();   // 作废在途搜索响应：清空后旧响应到达不得再落数据
    m_searchResults.clear();
    m_searchHasMore = false;
    m_searchPerformed = false;
    m_searchPage = 1;
    m_searchTotalPages = 0;
    m_lastKeyword.clear();
    m_searchLoading = false;   // 在途请求已被作废（其响应将被丢弃），加载标记在此收尾复位
    emit searchResultsChanged();
    emit searchHasMoreChanged();
    emit searchPerformedChanged();
    emit searchLoadingChanged();
}

QVariantMap RecipeViewModel::nutritionReport() const { return m_nutritionReport; }
bool RecipeViewModel::nutritionLoading() const { return m_nutritionLoading; }
QVariantList RecipeViewModel::recipeVideos() const { return m_recipeVideos; }
bool RecipeViewModel::videosLoading() const { return m_videosLoading; }
QVariantList RecipeViewModel::myRecipes() const { return m_myRecipes; }
bool RecipeViewModel::myRecipesLoading() const { return m_myRecipesLoading; }
bool RecipeViewModel::myRecipesHasMore() const { return m_myRecipesHasMore; }
QVariantList RecipeViewModel::recipeRatings() const { return m_recipeRatings; }
bool RecipeViewModel::ratingsLoading() const { return m_ratingsLoading; }
bool RecipeViewModel::ratingsHasMore() const { return m_ratingsHasMore; }
QVariantMap RecipeViewModel::myRating() const { return m_myRating; }

void RecipeViewModel::loadNutritionReport(int recipeId)
{
    m_detailRequestedId = recipeId;
    m_nutritionLoading = true;
    emit nutritionLoadingChanged();

    m_api->getRecipeNutrition(recipeId,
        [self = QPointer<RecipeViewModel>(this), recipeId](bool success, const gocook::models::NutritionReport& data, const std::string& error) {
            if (!self) return;
            if (recipeId != self->m_detailRequestedId) return;  // 过期响应丢弃
            if (!success) {
                // 详情页内子区块失败静默（详情页不监听 errorOccurred，无影响）；
                // 独立营养报告页监听 errorOccurred 呈现加载失败，区分「暂无报告」空状态
                emit self->errorOccurred(QString::fromStdString(
                    error.empty() ? QStringLiteral("加载营养报告失败").toStdString() : error));
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
    m_detailRequestedId = recipeId;
    m_videosLoading = true;
    emit videosLoadingChanged();

    m_api->getRecipeVideos(recipeId,
        [self = QPointer<RecipeViewModel>(this), recipeId](bool success, const std::vector<gocook::models::RecipeVideo>& data, const std::string& error) {
            if (!self) return;
            if (recipeId != self->m_detailRequestedId) return;  // 过期响应丢弃
            if (!success) {
                // 详情页子区块失败静默：主内容失败已由缓存兜底/离线视图呈现，
                // 子区块不再整页弹提示（避免与全局提示重复/遮挡；主流 App 为区内留空）
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

void RecipeViewModel::loadRecipeRatings(int recipeId, int page, int size)
{
    // 同步清空旧数据，防止二次打开时残留显示
    if (page == 1 && !m_recipeRatings.isEmpty()) {
        m_recipeRatings.clear();
        emit recipeRatingsChanged();
    }
    m_detailRequestedId = recipeId;
    m_ratingsLoading = true;
    m_ratingsRecipeId = recipeId;
    emit ratingsLoadingChanged();

    m_api->getRecipeRatings(recipeId, page, size,
        [self = QPointer<RecipeViewModel>(this), page, recipeId](bool success, const gocook::models::PagedRatings& data, const std::string& error) {
            if (!self) return;
            if (recipeId != self->m_detailRequestedId) return;  // 过期响应丢弃
            if (!success) {
                // 详情页子区块失败静默：主内容失败已由缓存兜底/离线视图呈现，
                // 子区块不再整页弹提示（避免与全局提示重复/遮挡；主流 App 为区内留空）
                self->m_ratingsLoading = false;
                emit self->ratingsLoadingChanged();
                return;
            }

            QVariantMap mapped = DataMapper::toMap(data);
            QVariantList ratings = mapped["data"].toList();
            QVariantMap pag = mapped["pagination"].toMap();

            if (page == 1)
                self->m_recipeRatings.clear();

            for (const auto& r : ratings)
                self->m_recipeRatings.append(r);

            self->m_ratingsPage = pag["page"].toInt();
            self->m_ratingsTotalPages = pag["total_pages"].toInt();
            self->m_ratingsHasMore = (self->m_ratingsPage < self->m_ratingsTotalPages);

            self->m_ratingsLoading = false;
            emit self->ratingsLoadingChanged();
            emit self->recipeRatingsChanged();
            emit self->ratingsHasMoreChanged();
        });
}

void RecipeViewModel::loadMoreRatings()
{
    if (m_ratingsLoading || !m_ratingsHasMore || m_ratingsRecipeId == 0) return;
    loadRecipeRatings(m_ratingsRecipeId, m_ratingsPage + 1, kRatingPageSize);
}

void RecipeViewModel::loadMyRecipeRating(int recipeId)
{
    m_detailRequestedId = recipeId;
    m_api->getMyRecipeRating(recipeId,
        [self = QPointer<RecipeViewModel>(this), recipeId](bool success,
                 const std::optional<gocook::models::RecipeRating>& data,
                 const std::string& error) {
            if (!self) return;
            if (recipeId != self->m_detailRequestedId) return;  // 过期响应丢弃
            if (!success) {
                // 404 表示未评分，清空并静默处理
                self->m_myRating = QVariantMap();
                emit self->myRatingChanged();
                return;
            }
            if (data.has_value()) {
                self->m_myRating = DataMapper::toMap(data.value());
            } else {
                self->m_myRating = QVariantMap();
            }
            emit self->myRatingChanged();
        });
}

void RecipeViewModel::rateRecipe(int recipeId, int rating, const QString& comment)
{
    gocook::models::RateRecipeRequest req;
    req.rating = rating;
    req.comment = comment.toStdString();

    m_api->rateRecipe(recipeId, req,
        [self = QPointer<RecipeViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->ratingError(QString::fromStdString(error));
                return;
            }
            emit self->ratingSubmitted();
        });
}

void RecipeViewModel::updateRating(int recipeId, int ratingId, int rating, const QString& comment)
{
    gocook::models::RateRecipeRequest req;
    req.rating = rating;
    req.comment = comment.toStdString();

    m_api->updateRating(recipeId, ratingId, req,
        [self = QPointer<RecipeViewModel>(this), ratingId](bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->ratingError(QString::fromStdString(error));
                return;
            }
            emit self->ratingUpdated(ratingId);
        });
}

void RecipeViewModel::deleteRating(int recipeId, int ratingId)
{
    // 乐观删除：保存旧值用于回滚
    QVariantMap oldMyRating = m_myRating;
    QVariantList oldRatings = m_recipeRatings;

    // 1. 立即清除本人的 myRating
    if (!m_myRating.isEmpty()) {
        m_myRating = QVariantMap();
        emit myRatingChanged();
    }

    // 2. 从列表中移除对应的评分项
    bool removed = false;
    for (int i = 0; i < m_recipeRatings.size(); ++i) {
        auto item = m_recipeRatings[i].toMap();
        if (item.value("id").toInt() == ratingId) {
            m_recipeRatings.removeAt(i);
            removed = true;
            break;
        }
    }
    if (removed)
        emit recipeRatingsChanged();

    // 3. 发 API 请求
    m_api->deleteRating(recipeId, ratingId,
        [self = QPointer<RecipeViewModel>(this), recipeId, ratingId,
         oldMyRating, oldRatings](bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                // 失败 → 回滚
                self->m_myRating = oldMyRating;
                if (!oldMyRating.isEmpty())
                    emit self->myRatingChanged();
                self->m_recipeRatings = oldRatings;
                emit self->recipeRatingsChanged();
                emit self->ratingError(QString::fromStdString(error));
                return;
            }
            // 成功 → 从服务端刷新确认（此时列表已乐观清除，直接重新加载）
            emit self->ratingDeleted(ratingId);
        });
}

void RecipeViewModel::deleteRecipe(int recipeId)
{
    QVariantList oldList = m_myRecipes;

    // 记录到"待删除"集合，后续任何 refresh 回调都会自动过滤
    m_pendingDeleteIds.insert(recipeId);

    // 构建新列表（不含目标条目），强制 QML 模型视为完全重建
    // 避免增量删除在删除/分页交错时出现残留或索引错位
    QVariantList newList;
    bool found = false;
    for (const auto& item : m_myRecipes) {
        if (item.toMap().value("id").toInt() == recipeId) {
            found = true;
            continue; // 跳过被删除项
        }
        newList.append(item);
    }

    if (found) {
        // 乐观删除：用全新 QVariantList 替换原列表
        m_myRecipes = newList;
        emit myRecipesChanged();
        emit recipeDeleted();
    } else {
        // 列表里没有该菜谱（分页未加载到）→ 直接 emit 信号
        emit recipeDeleted();
    }

    // 发 API 请求
    // 快照当前会话：登出/换号后到达的响应作废——失败回滚不得复活已清空的旧列表
    const auto session = SessionSnapshot::capture(m_api);
    m_api->deleteRecipe(recipeId,
        [self = QPointer<RecipeViewModel>(this), oldList, found, recipeId, session](bool success, const std::string& error) {
            if (!self) return;
            self->m_pendingDeleteIds.remove(recipeId); // 无论成功失败都清理
            // 会话已切换：过期响应作废（clearMyContent 已接管，不回滚、不重拉、不发信号）
            if (!session.isCurrent(self->m_api)) return;
            if (success) {
                // DELETE 成功后从服务端同步
                self->loadMyRecipes(1, self->m_myRecipesStatus);
            } else if (found) {
                // 失败 → 回滚
                self->m_myRecipes = oldList;
                emit self->myRecipesChanged();
                emit self->deleteFailed(QString::fromStdString(error));
            } else {
                emit self->deleteFailed(QString::fromStdString(error));
            }
        });
}

void RecipeViewModel::loadMyRecipes(int page, const QString& status, int size)
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

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应作废——
    // 我的投稿是个人数据且页面对游客可见，迟到响应不得落入已清空的列表
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getMySubmittedRecipes(page, size, status.toStdString(),
        [self = QPointer<RecipeViewModel>(this), page, session]
        (bool success, const gocook::models::PagedMyRecipes& data, const std::string& error) {
            if (!self) return;
            // 会话已切换：过期响应作废（不清状态，clearMyContent/新加载已接管）；
            // 仅复位加载标记防页面卡死（极端时序下 clearMyContent 尚未执行）
            if (!session.isCurrent(self->m_api)) {
                if (self->m_myRecipesLoading) {
                    self->m_myRecipesLoading = false;
                    emit self->myRecipesLoadingChanged();
                }
                return;
            }
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
                // 跳过仍在乐观删除中的条目（防止 GET 比 DELETE 快带来的竞态）
                if (self->m_pendingDeleteIds.contains(map.value("id").toInt()))
                    continue;
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
    loadMyRecipes(m_myRecipesPage + 1, m_myRecipesStatus);
}

QVariantList RecipeViewModel::myRatings() const { return m_myRatings; }
bool RecipeViewModel::myRatingsLoading() const { return m_myRatingsLoading; }
bool RecipeViewModel::myRatingsHasMore() const { return m_myRatingsHasMore; }

void RecipeViewModel::loadMyRatings(int page, int size)
{
    if (m_myRatingsLoading) return;

    m_myRatingsPage = page;
    m_myRatingsLoading = true;
    if (page == 1) {
        m_myRatings.clear();
        emit myRatingsChanged();
    }
    emit myRatingsLoadingChanged();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应作废——
    // 我的评论是个人数据且页面对游客可见，迟到响应不得落入已清空的列表
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getMyRatings(page, size,
        [self = QPointer<RecipeViewModel>(this), page, session]
        (bool success, const gocook::models::PagedUserRatings& data, const std::string& error) {
            if (!self) return;
            // 会话已切换：过期响应作废（不清状态，clearMyContent/新加载已接管）；
            // 仅复位加载标记防页面卡死（极端时序下 clearMyContent 尚未执行）
            if (!session.isCurrent(self->m_api)) {
                if (self->m_myRatingsLoading) {
                    self->m_myRatingsLoading = false;
                    emit self->myRatingsLoadingChanged();
                }
                return;
            }
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_myRatingsLoading = false;
                emit self->myRatingsLoadingChanged();
                return;
            }

            if (page == 1) {
                self->m_myRatings.clear();
            }

            for (const auto& item : data.data) {
                auto map = DataMapper::toMap(item);
                self->m_myRatings.append(map);
            }

            self->m_myRatingsPage = data.pagination.page;
            self->m_myRatingsTotalPages = data.pagination.total_pages;
            self->m_myRatingsHasMore = (self->m_myRatingsPage < self->m_myRatingsTotalPages);

            emit self->myRatingsChanged();
            emit self->myRatingsHasMoreChanged();
            self->m_myRatingsLoading = false;
            emit self->myRatingsLoadingChanged();
        });
}

void RecipeViewModel::loadMyRatingsNextPage()
{
    if (m_myRatingsLoading || !m_myRatingsHasMore) return;
    loadMyRatings(m_myRatingsPage + 1);
}

void RecipeViewModel::loadFavorites(int page, const QString &group, int size)
{
    if (m_favoritesLoading) return;
    m_favoritesGroupFilter = group;   // 记录筛选与页大小：loadMoreFavorites 续页必须沿用一致参数
    m_favoritesPageSize = size;
    m_favoritesLoading = true;
    m_favoritesLoadFailed = false;
    emit favoritesLoadFailedChanged();
    emit favoritesLoadingChanged();

    // 快照当前会话（token，SessionSnapshot）：响应到达时若会话已切换（登出/换号），该在途响应属于
    // 旧账号，静默丢弃——不清状态（clearFavorites 与新加载已接管），避免旧账号数据串入当前界面
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getFavorites(page, size, group.toStdString(),
                        [self = QPointer<RecipeViewModel>(this), page, session]
                        (bool success, const gocook::models::PagedFavorites& data,
                         const std::string& error) {
        if (!self) return;
        // 会话已切换：过期响应作废。不清数据（clearFavorites/新加载已接管），
        // 仅复位加载标记防页面卡死（极端时序下 clearFavorites 可能尚未执行）
        if (!session.isCurrent(self->m_api)) {
            if (self->m_favoritesLoading) {
                self->m_favoritesLoading = false;
                emit self->favoritesLoadingChanged();
            }
            return;
        }
        if (!success) {
            // 页面自行呈现：有旧数据则静默显示旧数据，无数据则居中离线视图（PDD 行为）
            // 守卫拦截（未登录，error=请先登录）：旧数据是上一登录态的私有数据，必须清空
            if (QString::fromStdString(error) == HttpGoCookApi::kAuthRequiredError) {
                self->m_favorites.clear();
                self->m_favoritesPage = 1;
                self->m_favoritesHasMore = false;
                emit self->favoritesChanged();
                emit self->favoritesHasMoreChanged();
            }
            self->m_favoritesLoadFailed = true;
            emit self->favoritesLoadFailedChanged();
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
    loadFavorites(m_favoritesPage + 1, m_favoritesGroupFilter, m_favoritesPageSize);
}

void RecipeViewModel::clearFavorites()
{
    m_favorites.clear();
    m_favoritesPage = 1;
    m_favoritesHasMore = false;
    m_favoritesTotalPages = 0;
    m_favoritesLoading = false;
    m_favoritesLoadFailed = false;
    // 收藏分组同样属于个人数据：登出后必须清空，否则游客在详情页点收藏时
    // 分组对话框仍显示上一账号的分组（loadFavoriteGroups 失败时旧数据被保留）
    m_favoriteGroups.clear();
    emit favoriteGroupsChanged();
    emit favoritesChanged();
    emit favoritesHasMoreChanged();
    emit favoritesLoadingChanged();
    emit favoritesLoadFailedChanged();
}

void RecipeViewModel::clearMyContent()
{
    // 我的投稿 / 我的评论属于个人数据，且两页对游客可见（未登录呈现登录空态）：
    // 登出后必须清空，否则残留会让上一账号的投稿/评论直接可见；
    // 分页与在途状态同步复位——loading 复位保证游客重进时入口清空不被重入守卫跳过，
    // 在途请求的迟到响应由会话快照判废（见 loadMyRecipes / loadMyRatings 回调）
    m_myRecipes.clear();
    m_myRecipesPage = 1;
    m_myRecipesTotalPages = 0;
    m_myRecipesHasMore = false;
    m_myRecipesStatus.clear();
    m_myRecipesLoading = false;
    m_pendingDeleteIds.clear();
    m_myRatings.clear();
    m_myRatingsPage = 1;
    m_myRatingsTotalPages = 0;
    m_myRatingsHasMore = false;
    m_myRatingsLoading = false;
    emit myRecipesChanged();
    emit myRecipesHasMoreChanged();
    emit myRecipesLoadingChanged();
    emit myRatingsChanged();
    emit myRatingsHasMoreChanged();
    emit myRatingsLoadingChanged();
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
            // 主请求（loadFavorites）未完成或已失败时静默：并发失败顺序不定，
            // 页面将由离线视图/旧数据呈现，避免与全局提示重复弹窗
            if (!self->m_favoritesLoading && !self->m_favoritesLoadFailed) {
                emit self->favoriteOperationFailed(QString::fromStdString(error));
            }
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
    if (groupId <= 0) {
        emit favoriteOperationFailed("默认分组不可删除");
        return;
    }

    // 乐观删除：保存旧列表用于回滚
    QVariantList oldGroups = m_favoriteGroups;
    QVariantList oldFavorites = m_favorites;

    // 找到目标分组的信息
    QString groupName;
    int groupIdx = -1;
    for (int i = 0; i < m_favoriteGroups.size(); ++i) {
        auto map = m_favoriteGroups[i].toMap();
        if (map.value("id").toInt() == groupId) {
            groupName = map.value("name").toString();
            groupIdx = i;
            break;
        }
    }

    if (groupIdx >= 0) {
        // 1. 从分组列表中移除
        m_favoriteGroups.removeAt(groupIdx);
        emit favoriteGroupsChanged();

        // 2. 移除该分组下的所有收藏条目
        QVariantList remaining;
        for (const auto &v : m_favorites) {
            if (v.toMap().value("groupName").toString() != groupName)
                remaining.append(v);
        }
        m_favorites = remaining;
        emit favoritesChanged();
    }

    // 3. 发 API 请求
    m_api->deleteFavoriteGroup(groupId,
        [self = QPointer<RecipeViewModel>(this), groupId,
         oldGroups, oldFavorites]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->favoriteGroupDeleted();
        } else {
            // 失败 → 回滚
            self->m_favoriteGroups = oldGroups;
            self->m_favorites = oldFavorites;
            emit self->favoriteGroupsChanged();
            emit self->favoritesChanged();
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
            emit self->favoriteRemoved();
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
            emit self->favoriteRemoved();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "批量删除失败" : error));
        }
    });
}

void RecipeViewModel::moveFavorite(int favoriteId, int groupId)
{
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {favoriteId};
    req.group_id = groupId;
    m_api->batchUpdateFavorites(req,
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
    gocook::models::BatchUpdateFavoritesRequest req;
    for (const auto &v : favoriteIds)
        req.favorite_ids.push_back(v.toInt());
    if (req.favorite_ids.empty()) return;
    req.group_id = groupId;
    m_api->batchUpdateFavorites(req,
        [self = QPointer<RecipeViewModel>(this)]
        (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->favoriteMoved();
        } else {
            emit self->favoriteOperationFailed(QString::fromStdString(
                error.empty() ? "批量移动失败" : error));
        }
    });
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

void RecipeViewModel::uploadRecipeImage(int recipeId, const QString& filePath)
{
    m_api->uploadRecipeImage(recipeId, filePath.toStdString(),
        [self = QPointer<RecipeViewModel>(this)](bool success,
              const std::string& imageUrl, const std::string& error) {
            if (!self) return;
            if (success) {
                emit self->recipeImageUploaded(QString::fromStdString(imageUrl));
            } else {
                emit self->recipeImageUploadFailed(QString::fromStdString(error));
            }
        });
}

void RecipeViewModel::uploadStepImage(int recipeId, int stepIndex, const QString& filePath)
{
    m_api->uploadStepImage(recipeId, stepIndex, filePath.toStdString(),
        [self = QPointer<RecipeViewModel>(this), stepIndex](bool success,
              const std::string& imageUrl, const std::string& error) {
            if (!self) return;
            if (success) {
                emit self->stepImageUploaded(stepIndex, QString::fromStdString(imageUrl));
            } else {
                emit self->stepImageUploadFailed(stepIndex, QString::fromStdString(error));
            }
        });
}
