#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>
#include <gocook/IGoCookApi.h>

class RecipeViewModel : public QObject
{
    Q_OBJECT
    // 暴露给 QML 的属性：菜谱列表
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY recipesChanged)
    // 加载状态
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    // 健康过滤是否已应用（来自推荐接口）
    Q_PROPERTY(bool healthFilterApplied READ healthFilterApplied NOTIFY healthFilterAppliedChanged)

public:
    // 依赖注入，传入抽象接口 IGoCookApi
    explicit RecipeViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList recipes() const;
    bool isLoading() const;
    bool healthFilterApplied() const;

    // 供 QML 调用的方法
    Q_INVOKABLE void loadPublicRecipes(int page = 1, int size = 20);
    Q_INVOKABLE void loadRecommendedRecipes(int page = 1, int size = 20);

signals:
    void recipesChanged();
    void isLoadingChanged();
    void healthFilterAppliedChanged();
    void errorOccurred(const QString &error);

private:
    void setHealthFilterApplied(bool applied);

    IGoCookApi *m_api;
    QVariantList m_recipes;
    bool m_isLoading = false;
    bool m_healthFilterApplied = false;
};