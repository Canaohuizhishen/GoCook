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

public:
    explicit RecipeViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList recipes() const;
    bool isLoading() const;
    bool hasMore() const;
    bool healthFilterApplied() const;
    QVariantMap recipeDetail() const;
    bool detailLoading() const;

    Q_INVOKABLE void loadPublicRecipes(int page = 1, int size = 20);
    Q_INVOKABLE void loadRecommendedRecipes(int page = 1, int size = 20);
    Q_INVOKABLE void loadNextPage();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadRecipeDetail(int recipeId);
    Q_INVOKABLE void submitRecipe(const QString& name, const QString& description,
                                   const QString& imageUrl, const QVariantList& ingredients,
                                   const QVariantList& steps, const QVariantList& tags);

signals:
    void recipesChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void healthFilterAppliedChanged();
    void recipeDetailChanged();
    void detailLoadingChanged();
    void errorOccurred(const QString &error);
    void recipeSubmitted(int id, const QString& status);
    void submitFailed(const QString& error);

private:
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
};
