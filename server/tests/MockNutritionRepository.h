#pragma once

#include <gocook/INutritionRepository.h>
#include <gmock/gmock.h>

/// 营养仓库 Mock：按 names 顺序返回注入的结果（供 RecipeServiceImpl 营养计算单测使用）
class MockNutritionRepository : public gocook::repository::INutritionRepository {
public:
    MOCK_METHOD(std::vector<std::optional<gocook::repository::IngredientNutrition>>,
                findByNames, (const std::vector<std::string>&), (override));
};
