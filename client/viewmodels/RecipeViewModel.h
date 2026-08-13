#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <memory>
#include <gocook/IGoCookApi.h>

/**
 * @brief 菜谱域 ViewModel：QML 侧唯一的菜谱数据入口（setContextProperty("recipeVM") 注入，main.cpp:56）。
 *
 * 所有 Q_INVOKABLE 均为异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新，
 * 失败经 signals（searchErrorOccurred / errorOccurred 等）通知 QML；
 * 底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
 */
class RecipeViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY recipesChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool healthFilterApplied READ healthFilterApplied NOTIFY healthFilterAppliedChanged)
    Q_PROPERTY(QVariantMap recipeDetail READ recipeDetail NOTIFY recipeDetailChanged)
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailLoadingChanged)
    Q_PROPERTY(bool detailLoadFailed READ detailLoadFailed NOTIFY detailLoadFailedChanged)
    Q_PROPERTY(bool favoritesLoadFailed READ favoritesLoadFailed NOTIFY favoritesLoadFailedChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(bool searchLoading READ searchLoading NOTIFY searchLoadingChanged)
    Q_PROPERTY(bool searchHasMore READ searchHasMore NOTIFY searchHasMoreChanged)
    Q_PROPERTY(bool searchPerformed READ searchPerformed NOTIFY searchPerformedChanged)   ///< 已执行过搜索：区分"无结果"与"尚未搜索"两种空态
    Q_PROPERTY(QVariantMap nutritionReport READ nutritionReport NOTIFY nutritionReportChanged)
    Q_PROPERTY(bool nutritionLoading READ nutritionLoading NOTIFY nutritionLoadingChanged)
    Q_PROPERTY(QVariantList recipeVideos READ recipeVideos NOTIFY recipeVideosChanged)
    Q_PROPERTY(bool videosLoading READ videosLoading NOTIFY videosLoadingChanged)
    Q_PROPERTY(QVariantList recipeRatings READ recipeRatings NOTIFY recipeRatingsChanged)
    Q_PROPERTY(bool ratingsLoading READ ratingsLoading NOTIFY ratingsLoadingChanged)
    Q_PROPERTY(bool ratingsHasMore READ ratingsHasMore NOTIFY ratingsHasMoreChanged)
    Q_PROPERTY(QVariantMap myRating READ myRating NOTIFY myRatingChanged)
    Q_PROPERTY(QVariantList myRecipes READ myRecipes NOTIFY myRecipesChanged)
    Q_PROPERTY(bool myRecipesLoading READ myRecipesLoading NOTIFY myRecipesLoadingChanged)
    Q_PROPERTY(bool myRecipesHasMore READ myRecipesHasMore NOTIFY myRecipesHasMoreChanged)
    Q_PROPERTY(QVariantList myRatings READ myRatings NOTIFY myRatingsChanged)
    Q_PROPERTY(bool myRatingsLoading READ myRatingsLoading NOTIFY myRatingsLoadingChanged)
    Q_PROPERTY(bool myRatingsHasMore READ myRatingsHasMore NOTIFY myRatingsHasMoreChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(int favoritesTotalCount READ favoritesTotalCount NOTIFY favoritesChanged)   ///< 总数变化复用 favoritesChanged 通知
    Q_PROPERTY(bool favoritesHasMore READ favoritesHasMore NOTIFY favoritesHasMoreChanged)
    Q_PROPERTY(bool favoritesLoading READ favoritesLoading NOTIFY favoritesLoadingChanged)
    Q_PROPERTY(QVariantList favoriteGroups READ favoriteGroups NOTIFY favoriteGroupsChanged)
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl CONSTANT)   ///< CONSTANT：服务端地址（自 httpApi），QML 用于拼接资源 URL

public:
    explicit RecipeViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QString apiBaseUrl() const;
    QVariantList recipes() const;
    bool isLoading() const;
    bool hasMore() const;
    bool healthFilterApplied() const;
    QVariantMap recipeDetail() const;
    bool detailLoading() const;
    bool detailLoadFailed() const;
    bool favoritesLoadFailed() const;
    QVariantList searchResults() const;
    bool searchLoading() const;
    bool searchHasMore() const;
    bool searchPerformed() const;
    QVariantMap nutritionReport() const;
    bool nutritionLoading() const;
    QVariantList recipeVideos() const;
    bool videosLoading() const;
    QVariantList recipeRatings() const;
    bool ratingsLoading() const;
    bool ratingsHasMore() const;
    QVariantMap myRating() const;
    QVariantList myRecipes() const;
    bool myRecipesLoading() const;
    bool myRecipesHasMore() const;
    QVariantList myRatings() const;
    bool myRatingsLoading() const;
    bool myRatingsHasMore() const;
    QVariantList favorites() const { return m_favorites; }
    int favoritesTotalCount() const { return m_favoritesTotal; }
    bool favoritesHasMore() const { return m_favoritesHasMore; }
    bool favoritesLoading() const { return m_favoritesLoading; }
    QVariantList favoriteGroups() const { return m_favoriteGroups; }

    // ===== 菜谱列表与详情 =====
    Q_INVOKABLE void loadPublicRecipes(int page = 1, int size = 30);
    Q_INVOKABLE void loadRecommendedRecipes(int page = 1, int size = 30);
    Q_INVOKABLE void loadNextPage();   ///< 加载当前列表模式（公开/推荐）的下一页
    Q_INVOKABLE void refresh();   ///< 重新加载当前模式的第一页
    Q_INVOKABLE void loadRecipeDetail(int recipeId);
    // ===== 投稿与编辑 =====
    Q_INVOKABLE void submitRecipe(const QString& name, const QString& description,
                                   const QString& imageUrl, const QVariantList& ingredients,
                                   const QVariantList& steps, const QVariantList& tags);
    Q_INVOKABLE void uploadRecipeImage(int recipeId, const QString& filePath);
    Q_INVOKABLE void uploadStepImage(int recipeId, int stepIndex, const QString& filePath);
    Q_INVOKABLE void editRecipe(int recipeId, const QString& name, const QString& description,
                                 const QString& imageUrl, const QVariantList& ingredients,
                                 const QVariantList& steps, const QVariantList& tags);
    Q_INVOKABLE void loadRecipeForEdit(int recipeId);
    // ===== 搜索 =====
    Q_INVOKABLE void searchRecipes(const QString& keyword, int page = 1, int size = 20);
    Q_INVOKABLE void searchNextPage();
    Q_INVOKABLE void resetSearch();   ///< 清空搜索结果与空态标记（返回/清空输入时调用）
    // ===== 营养 / 视频 / 评分评论 =====
    Q_INVOKABLE void loadNutritionReport(int recipeId);
    Q_INVOKABLE void loadRecipeVideos(int recipeId);
    Q_INVOKABLE void loadRecipeRatings(int recipeId, int page = 1, int size = 10);
    Q_INVOKABLE void loadMoreRatings();
    Q_INVOKABLE void loadMyRecipeRating(int recipeId);
    Q_INVOKABLE void rateRecipe(int recipeId, int rating, const QString& comment);
    Q_INVOKABLE void updateRating(int recipeId, int ratingId, int rating, const QString& comment);
    Q_INVOKABLE void deleteRating(int recipeId, int ratingId);
    Q_INVOKABLE void deleteRecipe(int recipeId);   ///< 删除我的投稿（乐观删除，失败发 deleteFailed）
    // ===== 我的投稿与评论 =====
    Q_INVOKABLE void loadMyRecipes(int page = 1, int size = 20, const QString& status = "");   ///< status: "pending"/"approved"/"rejected"，空串=全部
    Q_INVOKABLE void loadMyRecipesNextPage();
    Q_INVOKABLE void loadMyRatings(int page = 1, int size = 20);
    Q_INVOKABLE void loadMyRatingsNextPage();

    // ===== 收藏 =====
    Q_INVOKABLE void loadFavorites(int page = 1, int size = 20, const QString &group = "");
    Q_INVOKABLE void loadMoreFavorites();
    Q_INVOKABLE void toggleFavorite(int recipeId, int groupId = 0);   ///< groupId=0 表示默认收藏夹
    Q_INVOKABLE void loadFavoriteGroups();
    Q_INVOKABLE void createFavoriteGroup(const QString &name);
    Q_INVOKABLE void deleteFavoriteGroup(int groupId);
    Q_INVOKABLE void removeFavorite(int favoriteId);
    Q_INVOKABLE void batchRemoveFavorites(const QVariantList &favoriteIds);   ///< 批量删除，完成后发 favoriteRemoved
    Q_INVOKABLE void moveFavorite(int favoriteId, int groupId);
    Q_INVOKABLE void batchMoveFavorites(const QVariantList &favoriteIds, int groupId);   ///< 批量移动，完成后发 favoriteMoved
    Q_INVOKABLE void updateFavoriteGroupName(int groupId, const QString &name);

signals:
    void favoriteMoved();
    void recipesChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void healthFilterAppliedChanged();
    void recipeDetailChanged();
    void detailLoadingChanged();
    void detailLoadFailedChanged();
    void favoritesLoadFailedChanged();
    void searchResultsChanged();
    void searchLoadingChanged();
    void searchHasMoreChanged();
    void searchPerformedChanged();
    void nutritionReportChanged();
    void nutritionLoadingChanged();
    void recipeVideosChanged();
    void videosLoadingChanged();
    void recipeRatingsChanged();
    void ratingsLoadingChanged();
    void ratingsHasMoreChanged();
    void myRatingChanged();
    void ratingSubmitted();
    void ratingUpdated(int ratingId);
    void ratingDeleted(int ratingId);
    void ratingError(const QString& error);
    void recipeDeleted();
    void deleteFailed(const QString& error);
    void myRecipesChanged();
    void myRecipesLoadingChanged();
    void myRecipesHasMoreChanged();
    void myRatingsChanged();
    void myRatingsLoadingChanged();
    void myRatingsHasMoreChanged();
    void searchErrorOccurred(const QString &error);
    void errorOccurred(const QString &error);
    void recipeSubmitted(int id, const QString& status);
    void submitFailed(const QString& error);
    void recipeImageUploaded(const QString& imageUrl);
    void recipeImageUploadFailed(const QString& error);
    void stepImageUploaded(int stepIndex, const QString& imageUrl);
    void stepImageUploadFailed(int stepIndex, const QString& error);
    void recipeEdited();
    void editFailed(const QString& error);
    void editFormDataReady();   ///< loadRecipeForEdit 成功、编辑表单数据已就绪
    void favoritesChanged();
    void favoritesHasMoreChanged();
    void favoritesLoadingChanged();
    void favoriteGroupsChanged();
    void favoriteToggleSuccess(int recipeId, bool isFavorited);   ///< 收藏成功（isFavorited=最终状态，当前恒 true）；取消收藏发 favoriteRemoved
    void favoriteRemoved();
    void favoriteGroupCreated();
    void favoriteGroupDeleted();
    void favoriteOperationFailed(const QString &error);

private:
    enum class LoadMode { Public, Recommended };
    LoadMode m_currentMode = LoadMode::Public;

    void setHealthFilterApplied(bool applied);

    IGoCookApi *m_api;
    QVariantList m_recipes;
    QVariantMap m_recipeDetail;
    bool m_isLoading = false;
    bool m_hasMore = false;
    bool m_healthFilterApplied = false;
    bool m_detailLoading = false;
    bool m_detailLoadFailed = false;   ///< 详情加载失败且无缓存（页面显示居中离线视图）
    int m_detailRequestedId = -1;      ///< 当前详情页所属菜谱 id：A→B 快速切换时丢弃 A 的过期响应（竞态防护）
    int m_currentPage = 1;
    int m_pageSize = 30;
    int m_totalPages = 0;

    // 搜索状态
    QVariantList m_searchResults;
    bool m_searchLoading = false;
    bool m_searchHasMore = false;
    bool m_searchPerformed = false;
    int m_searchPage = 1;
    int m_searchTotalPages = 0;
    QString m_lastKeyword;

    // 营养报告状态
    QVariantMap m_nutritionReport;
    bool m_nutritionLoading = false;

    // 视频状态
    QVariantList m_recipeVideos;
    bool m_videosLoading = false;

    // 评分评论状态
    QVariantList m_recipeRatings;
    bool m_ratingsLoading = false;
    bool m_ratingsHasMore = false;
    QVariantMap m_myRating;
    int m_ratingsPage = 1;
    int m_ratingsTotalPages = 0;
    int m_ratingsRecipeId = 0;

    // 我的投稿状态
    QVariantList m_myRecipes;
    bool m_myRecipesLoading = false;
    bool m_myRecipesHasMore = false;
    int m_myRecipesPage = 1;
    int m_myRecipesTotalPages = 0;
    QString m_myRecipesStatus;
    QSet<int> m_pendingDeleteIds;  ///< 乐观删除中但 API 尚未返回的菜谱 ID

    // 我的评论状态
    QVariantList m_myRatings;
    bool m_myRatingsLoading = false;
    bool m_myRatingsHasMore = false;
    int m_myRatingsPage = 1;
    int m_myRatingsTotalPages = 0;

    // 收藏状态
    QVariantList m_favorites;
    QVariantList m_favoriteGroups;
    int m_favoritesPage = 1;
    int m_favoritesTotalPages = 0;
    int m_favoritesTotal = 0;
    bool m_favoritesHasMore = false;
    bool m_favoritesLoading = false;
    bool m_favoritesLoadFailed = false; ///< 收藏加载失败（页面显示居中离线视图或静默保留旧数据）
};
