#include "RecipeViewModel.h"
#include <DataMapper.h>
#include <QDebug>

RecipeViewModel::RecipeViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList RecipeViewModel::recipes() const { return m_recipes; }
bool RecipeViewModel::isLoading() const { return m_isLoading; }

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
                                    m_recipes.append(DataMapper::toMap(recipe));
                                }

                                emit recipesChanged();
                                m_isLoading = false;
                                emit isLoadingChanged();
                            });
}

void RecipeViewModel::loadRecommendedRecipes(int page, int size)
{
    Q_UNUSED(page)
    Q_UNUSED(size)
    // TODO: 对接智能推荐接口
    qDebug() << "loadRecommendedRecipes not implemented yet";
    emit errorOccurred("Recommended recipes not implemented yet");
}