#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <gocook/IGoCookApi.h>

class RecipeViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY recipesChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(bool healthFilterApplied READ healthFilterApplied NOTIFY healthFilterAppliedChanged)
    Q_PROPERTY(QVariantMap recipeDetail READ recipeDetail NOTIFY recipeDetailChanged)
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailLoadingChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(bool searchLoading READ searchLoading NOTIFY searchLoadingChanged)
    Q_PROPERTY(bool searchHasMore READ searchHasMore NOTIFY searchHasMoreChanged)
    Q_PROPERTY(bool searchPerformed READ searchPerformed NOTIFY searchPerformedChanged)
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

public:
    explicit RecipeViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList recipes() const;
    bool isLoading() const;
    bool hasMore() const;
    bool healthFilterApplied() const;
    QVariantMap recipeDetail() const;
    bool detailLoading() const;
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

    Q_INVOKABLE void loadPublicRecipes(int page = 1, int size = 20);
    Q_INVOKABLE void loadRecommendedRecipes(int page = 1, int size = 20);
    Q_INVOKABLE void loadNextPage();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadRecipeDetail(int recipeId);
    Q_INVOKABLE void submitRecipe(const QString& name, const QString& description,
                                   const QString& imageUrl, const QVariantList& ingredients,
                                   const QVariantList& steps, const QVariantList& tags);
    Q_INVOKABLE void searchRecipes(const QString& keyword, int page = 1, int size = 20);
    Q_INVOKABLE void searchNextPage();
    Q_INVOKABLE void resetSearch();
    Q_INVOKABLE void loadNutritionReport(int recipeId);
    Q_INVOKABLE void loadRecipeVideos(int recipeId);
    Q_INVOKABLE void loadRecipeRatings(int recipeId, int page = 1, int size = 10);
    Q_INVOKABLE void loadMoreRatings();
    Q_INVOKABLE void loadMyRecipeRating(int recipeId);
    Q_INVOKABLE void rateRecipe(int recipeId, int rating, const QString& comment);
    Q_INVOKABLE void updateRating(int recipeId, int ratingId, int rating, const QString& comment);
    Q_INVOKABLE void deleteRating(int recipeId, int ratingId);
    Q_INVOKABLE void loadMyRecipes(int page = 1, int size = 20, const QString& status = "");
    Q_INVOKABLE void loadMyRecipesNextPage();
    Q_INVOKABLE void loadMyRatings(int page = 1, int size = 20);
    Q_INVOKABLE void loadMyRatingsNextPage();

signals:
    void recipesChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void healthFilterAppliedChanged();
    void recipeDetailChanged();
    void detailLoadingChanged();
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
    int m_currentPage = 1;
    int m_pageSize = 20;
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

    // 我的评论状态
    QVariantList m_myRatings;
    bool m_myRatingsLoading = false;
    bool m_myRatingsHasMore = false;
    int m_myRatingsPage = 1;
    int m_myRatingsTotalPages = 0;
};
