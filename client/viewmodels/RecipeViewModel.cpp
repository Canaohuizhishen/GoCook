#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>

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
                            [this](bool success, const gocook::models::PagedRecipes& data, const std::string& error) {
                                if (!success) {
                                    emit errorOccurred(QString::fromStdString(error));
                                    m_isLoading = false;
                                    emit isLoadingChanged();
                                    return;
                                }

                                m_recipes.clear();
                                for (const auto& recipe : data.data) {
                                    // 使用统一映射函数，替代手工逐字段赋值
                                    auto item = DataMapper::toMap(recipe);
                                    // 补充 API v2.7 新增的属性字段，保持 QML 侧驼峰命名
                                    item["cookingMethod"]  = QString::fromStdString(recipe.cooking_method);
                                    item["flavor"]         = QString::fromStdString(recipe.flavor);
                                    item["ingredientType"] = QString::fromStdString(recipe.ingredient_type);
                                    item["calories"]       = recipe.calories;
                                    item["viewCount"]      = recipe.view_count;
                                    item["avgRating"]      = recipe.avg_rating;
                                    m_recipes.append(item);
                                }

                                emit recipesChanged();
                                m_isLoading = false;
                                emit isLoadingChanged();
                            });
}

void RecipeViewModel::loadRecommendedRecipes(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getRecommendedRecipes(page, size,
                                 [this](bool success, const gocook::models::PagedRecommendedRecipes& data, const std::string& error) {
                                     if (!success) {
                                         emit errorOccurred(QString::fromStdString(error));
                                         m_isLoading = false;
                                         emit isLoadingChanged();
                                         return;
                                     }

                                     // 更新健康过滤标志
                                     setHealthFilterApplied(data.health_filter_applied);

                                     m_recipes.clear();
                                     for (const auto& recipe : data.data) {
                                         // 使用推荐菜谱映射（含 match_score / match_status），然后补充基础字段
                                         auto item = DataMapper::toMap(recipe);
                                         // 补充菜谱基础属性，对齐 API v2.7/v2.8，确保字段齐全
                                         item["cookingMethod"]  = QString::fromStdString(recipe.cooking_method);
                                         item["flavor"]         = QString::fromStdString(recipe.flavor);
                                         item["ingredientType"] = QString::fromStdString(recipe.ingredient_type);
                                         item["calories"]       = recipe.calories;
                                         item["viewCount"]      = recipe.view_count;
                                         item["avgRating"]      = recipe.avg_rating;
                                         m_recipes.append(item);
                                     }

                                     emit recipesChanged();
                                     m_isLoading = false;
                                     emit isLoadingChanged();
                                 });
}