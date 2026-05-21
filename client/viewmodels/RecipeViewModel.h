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
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(int favoritesTotalCount READ favoritesTotalCount NOTIFY favoritesChanged)
    Q_PROPERTY(bool favoritesHasMore READ favoritesHasMore NOTIFY favoritesHasMoreChanged)
    Q_PROPERTY(bool favoritesLoading READ favoritesLoading NOTIFY favoritesLoadingChanged)
    Q_PROPERTY(QVariantList favoriteGroups READ favoriteGroups NOTIFY favoriteGroupsChanged)

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
    QVariantList favorites() const { return m_favorites; }
    int favoritesTotalCount() const { return m_favoritesTotal; }
    bool favoritesHasMore() const { return m_favoritesHasMore; }
    bool favoritesLoading() const { return m_favoritesLoading; }
    QVariantList favoriteGroups() const { return m_favoriteGroups; }

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

    Q_INVOKABLE void loadFavorites(int page = 1, int size = 20, const QString &group = "");
    Q_INVOKABLE void loadMoreFavorites();
    Q_INVOKABLE void toggleFavorite(int recipeId, int groupId = 0);
    Q_INVOKABLE void loadFavoriteGroups();
    Q_INVOKABLE void createFavoriteGroup(const QString &name);
    Q_INVOKABLE void deleteFavoriteGroup(int groupId);
    Q_INVOKABLE void removeFavorite(int favoriteId);
    Q_INVOKABLE void batchRemoveFavorites(const QVariantList &favoriteIds);
    Q_INVOKABLE void moveFavorite(int favoriteId, int groupId);
    Q_INVOKABLE void batchMoveFavorites(const QVariantList &favoriteIds, int groupId);
    Q_INVOKABLE void updateFavoriteGroupName(int groupId, const QString &name);

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
    void searchErrorOccurred(const QString &error);
    void errorOccurred(const QString &error);
    void recipeSubmitted(int id, const QString& status);
    void submitFailed(const QString& error);
    void favoritesChanged();
    void favoritesHasMoreChanged();
    void favoritesLoadingChanged();
    void favoriteGroupsChanged();
    void favoriteToggleSuccess(int recipeId, bool isFavorited);
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

    // 收藏状态
    QVariantList m_favorites;
    QVariantList m_favoriteGroups;
    int m_favoritesPage = 1;
    int m_favoritesTotalPages = 0;
    int m_favoritesTotal = 0;
    bool m_favoritesHasMore = false;
    bool m_favoritesLoading = false;
};
