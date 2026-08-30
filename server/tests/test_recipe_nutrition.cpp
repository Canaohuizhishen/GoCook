#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../services/RecipeServiceImpl.h"
#include "MockRecipeRepository.h"
#include "MockNutritionRepository.h"

using namespace testing;
using namespace gocook::models;
using namespace gocook::services;
using namespace gocook::repository;

namespace {

    // 便捷构造：营养条目（每 100g）
    IngredientNutrition makeNut(const std::string& name, double cal, double pro,
                                double fat, double carb, double portionG = 0.0,
                                double fiber = 0.0, double sodium = 0.0, double vitc = 0.0) {
        IngredientNutrition n;
        n.name = name;
        n.calories = cal;
        n.protein_g = pro;
        n.fat_g = fat;
        n.carbs_g = carb;
        n.default_portion_g = portionG;
        n.fiber_g = fiber;
        n.sodium_mg = sodium;
        n.vitamin_c_mg = vitc;
        return n;
    }

    Ingredient makeIng(const std::string& name, double qty, const std::string& unit) {
        Ingredient ing;
        ing.name = name;
        ing.quantity = qty;
        ing.unit = unit;
        return ing;
    }

    // 单食材投稿请求
    SubmitRecipeRequest makeReq(const std::vector<Ingredient>& ings) {
        SubmitRecipeRequest req;
        req.name = "测试菜谱";
        req.description = "desc";
        req.ingredients = ings;
        return req;
    }

    // 校验 create 收到的营养 JSON 的核心字段
    void expectNutritionJson(const nlohmann::json& j, double cal, double pro, double fat, double carb) {
        ASSERT_TRUE(j.is_object()) << "营养 JSON 应为对象";
        ASSERT_TRUE(j.contains("per_serving")) << "应包含 per_serving";
        EXPECT_DOUBLE_EQ(j["per_serving"]["calories"].get<double>(), cal);
        EXPECT_DOUBLE_EQ(j["per_serving"]["protein_g"].get<double>(), pro);
        EXPECT_DOUBLE_EQ(j["per_serving"]["fat_g"].get<double>(), fat);
        EXPECT_DOUBLE_EQ(j["per_serving"]["carbs_g"].get<double>(), carb);
        // 顶部四项与 per_serving 一致
        EXPECT_DOUBLE_EQ(j["calories"].get<double>(), cal);
    }

    // 默认返回"按入参名匹配"的实现：names 中每个名字都返回同名营养条目
    // （测试里如需别名/未匹配行为，用 EXPECT_CALL 单独覆盖）
    auto defaultMatcher = [](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) {
            IngredientNutrition nut;
            nut.name = n;
            nut.calories = 100.0; nut.protein_g = 10.0;
            nut.fat_g = 5.0; nut.carbs_g = 10.0;
            rows.push_back(nut);
        }
        return rows;
    };

    // 便捷构造：单食材投稿，返回入库营养 JSON（供健康提示等断言用）
    nlohmann::json submitSingle(const IngredientNutrition& nut, const Ingredient& ing) {
        auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
        auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
        auto* repo = recipeMock.get();
        EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([&](const std::vector<std::string>& names) {
            std::vector<std::optional<IngredientNutrition>> rows;
            for (const auto& n : names) rows.push_back(nut);
            return rows;
        }));
        EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
        nlohmann::json captured;
        EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
            captured = j;
            return SubmitRecipeResponse{1, "pending"};
        }));
        RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
        service.submitRecipe(42, makeReq({ing}));
        return captured;
    }
}

// ═══════════════════════════════════════════════════════════════
// 营养自动计算：基本换算与累加
// ═══════════════════════════════════════════════════════════════

TEST(RecipeNutritionTest, 克单位按每100g含量缩放) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke(defaultMatcher));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 鸡胸肉 100克：cal=100*1, pro=10*1, fat=5*1, carb=10*1
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("鸡胸肉", 100, "克")}));
    expectNutritionJson(captured, 100.0, 10.0, 5.0, 10.0);
}

TEST(RecipeNutritionTest, 多食材累加与明细) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) {
            IngredientNutrition nut;
            nut.name = n;
            nut.calories = 100.0; nut.protein_g = 10.0;
            nut.fat_g = 5.0; nut.carbs_g = 10.0;
            rows.push_back(nut);
        }
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 食材A 200克 + 食材B 50克：cal=200+50=250
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("A", 200, "克"), makeIng("B", 50, "克")}));
    expectNutritionJson(captured, 250.0, 25.0, 12.5, 25.0);

    ASSERT_TRUE(captured["ingredients_breakdown"].is_array());
    ASSERT_EQ(captured["ingredients_breakdown"].size(), 2u);
    EXPECT_DOUBLE_EQ(captured["ingredients_breakdown"][0]["calories"].get<double>(), 200.0);
    EXPECT_DOUBLE_EQ(captured["ingredients_breakdown"][1]["calories"].get<double>(), 50.0);
}

TEST(RecipeNutritionTest, 计数单位使用默认单重) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 鸡蛋：每100g 144千卡，单重50g；3个 → 150g → 144*1.5=216
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("鸡蛋", 144.0, 13.3, 8.8, 2.8, 50.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("鸡蛋", 3, "个")}));
    expectNutritionJson(captured, 216.0, 20.0, 13.2, 4.2);  // 蛋白 13.3×1.5=19.95 → 四舍五入 20.0
}

TEST(RecipeNutritionTest, 斤两毫升换算) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 1斤=500g → 500；2两=100g → 100；250毫升≈250g → 250；合计 850
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 1, "斤"), makeIng("X", 2, "两"), makeIng("X", 250, "毫升")}));
    expectNutritionJson(captured, 850.0, 85.0, 42.5, 85.0);
}

TEST(RecipeNutritionTest, 千克磅升换算) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 1千克=1000g→1000；1磅=453.592g→453.6；1升=1000ml≈1000g→1000；合计 2453.6
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 1, "千克"), makeIng("X", 1, "磅"), makeIng("X", 1, "升")}));
    expectNutritionJson(captured, 2453.6, 245.4, 122.7, 245.4);
}

TEST(RecipeNutritionTest, 单位大小写变体) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // g/G/mL/L/l/kg 全部识别：100+200+300+1000+500+2000 = 4100
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 100, "g"), makeIng("X", 200, "G"),
                                      makeIng("X", 300, "ml"), makeIng("X", 1, "L"),
                                      makeIng("X", 0.5, "l"), makeIng("X", 2, "kg")}));
    expectNutritionJson(captured, 4100.0, 410.0, 205.0, 410.0);
}

TEST(RecipeNutritionTest, 单位KG与ML大写及尾随空格) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // KG=2000、ML=300、"克 " 尾随空格按 trim 后 400、LB=453.6（1lb≈453.592 → round1 453.6）
    // → 合计 3153.6（此前 KG/ML/LB/带空格全部被跳过）
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 2, "KG"), makeIng("X", 300, "ML"),
                                      makeIng("X", 400, "克 "), makeIng("X", 1, "LB")}));
    expectNutritionJson(captured, 3153.6, 315.4, 157.7, 315.4);
}

TEST(RecipeNutritionTest, 容器单位体积近似) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 1杯=240g、2碗=400g、1汤匙=15g、2茶匙=10g、3勺=30g → 合计 695（脂肪 34.75 → 34.8）
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 1, "杯"), makeIng("X", 2, "碗"),
                                      makeIng("X", 1, "汤匙"), makeIng("X", 2, "茶匙"),
                                      makeIng("X", 3, "勺")}));
    expectNutritionJson(captured, 695.0, 69.5, 34.8, 69.5);
}

TEST(RecipeNutritionTest, 未知单位跳过) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("X", 100.0, 10.0, 5.0, 10.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // "扎"不在已知单位表 → 跳过；仅 100克 计入
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("X", 100, "克"), makeIng("X", 2, "扎")}));
    expectNutritionJson(captured, 100.0, 10.0, 5.0, 10.0);
    ASSERT_EQ(captured["ingredients_breakdown"].size(), 1u);
}

// ═══════════════════════════════════════════════════════════════
// 名称规范化 / 别名 / 未匹配
// ═══════════════════════════════════════════════════════════════

TEST(RecipeNutritionTest, 名称规范化去除括注) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 断言传给仓库的是去括注后的名字
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        EXPECT_EQ(names.size(), 1u);
        EXPECT_EQ(names[0], "鸡胸肉");   // "鸡胸肉（去皮）" → "鸡胸肉"
        std::vector<std::optional<IngredientNutrition>> rows;
        rows.push_back(makeNut("鸡胸肉", 165.0, 31.0, 3.6, 0.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Return(SubmitRecipeResponse{1, "pending"}));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("鸡胸肉（去皮）", 100, "克")}));
}

TEST(RecipeNutritionTest, 别名命中显示规范名) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 仓库按别名匹配后返回规范名"鸡蛋"
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        EXPECT_EQ(names[0], "土鸡蛋");
        std::vector<std::optional<IngredientNutrition>> rows;
        rows.push_back(makeNut("鸡蛋", 144.0, 13.3, 8.8, 2.8, 50.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("土鸡蛋", 2, "个")}));
    ASSERT_EQ(captured["ingredients_breakdown"][0]["name"].get<std::string>(), "鸡蛋");
}

TEST(RecipeNutritionTest, 部分未匹配只计算匹配项) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) {
            if (n == "神秘食材")
                rows.push_back(std::nullopt);
            else
                rows.push_back(makeNut(n, 100.0, 10.0, 5.0, 10.0));
        }
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 鸡胸肉 100克(匹配) + 神秘食材 100克(未收录) → 只算鸡胸肉
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("鸡胸肉", 100, "克"), makeIng("神秘食材", 100, "克")}));
    expectNutritionJson(captured, 100.0, 10.0, 5.0, 10.0);
    ASSERT_EQ(captured["ingredients_breakdown"].size(), 1u);
}

TEST(RecipeNutritionTest, 全部未匹配得到空对象) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows(names.size(), std::nullopt);
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("神秘食材", 100, "克")}));
    EXPECT_TRUE(captured.is_object());
    EXPECT_TRUE(captured.empty());  // 空对象 {} → 前端显示"暂无营养报告"
}

TEST(RecipeNutritionTest, 全部未匹配但有手填值时回退用户四项) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        return std::vector<std::optional<IngredientNutrition>>(names.size(), std::nullopt);
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 自动计算全部失败 → "用户可选补充"语义：回退使用手填四项（fiber/sodium/vitc 置 0），
    // 并附未计入明细（不再静默丢弃）
    auto req = makeReq({makeIng("神秘食材", 100, "克"), makeIng("神秘调料", 5, "克")});
    Nutrition userNut;
    userNut.calories = 321; userNut.protein = 22; userNut.fat = 12; userNut.carbs = 33;
    req.nutrition = userNut;

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, req);
    EXPECT_DOUBLE_EQ(captured["calories"].get<double>(), 321.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["calories"].get<double>(), 321.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["protein_g"].get<double>(), 22.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["fat_g"].get<double>(), 12.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["carbs_g"].get<double>(), 33.0);
    EXPECT_TRUE(captured["ingredients_breakdown"].is_array());
    EXPECT_TRUE(captured["ingredients_breakdown"].empty());
    // 全部食材未收录 → excluded 逐项列出
    ASSERT_TRUE(captured["excluded_ingredients"].is_array());
    ASSERT_EQ(captured["excluded_ingredients"].size(), 2u);
    EXPECT_EQ(captured["excluded_ingredients"][0]["name"].get<std::string>(), "神秘食材");
    EXPECT_EQ(captured["excluded_ingredients"][0]["reason"].get<std::string>(), "未收录");
}

TEST(RecipeNutritionTest, 部分匹配时标记未计入食材) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 鸡胸肉(匹配) + 神秘食材(未收录) + 番茄 2个(无单重→无法换算)
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) {
            if (n == "鸡胸肉")
                rows.push_back(makeNut("鸡胸肉", 165.0, 31.0, 3.6, 0.0));
            else if (n == "番茄")
                rows.push_back(makeNut("番茄", 20.0, 0.9, 0.2, 4.0));   // 无单重
            else
                rows.push_back(std::nullopt);
        }
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("鸡胸肉", 100, "克"),
                                      makeIng("神秘食材", 100, "克"),
                                      makeIng("番茄", 2, "个")}));
    // 只算鸡胸肉 100g
    expectNutritionJson(captured, 165.0, 31.0, 3.6, 0.0);
    // 未收录 + 无法换算 逐项标记
    ASSERT_TRUE(captured["excluded_ingredients"].is_array());
    ASSERT_EQ(captured["excluded_ingredients"].size(), 2u);
    EXPECT_EQ(captured["excluded_ingredients"][0]["name"].get<std::string>(), "神秘食材");
    EXPECT_EQ(captured["excluded_ingredients"][0]["reason"].get<std::string>(), "未收录");
    EXPECT_EQ(captured["excluded_ingredients"][1]["name"].get<std::string>(), "番茄");
    EXPECT_EQ(captured["excluded_ingredients"][1]["reason"].get<std::string>(), "无法换算");
}

TEST(RecipeNutritionTest, 全部匹配时排除项为空数组) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke(defaultMatcher));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("A", 100, "克"), makeIng("B", 50, "克")}));
    ASSERT_TRUE(captured.contains("excluded_ingredients"));
    EXPECT_TRUE(captured["excluded_ingredients"].is_array());
    EXPECT_TRUE(captured["excluded_ingredients"].empty());
}

TEST(RecipeNutritionTest, 营养仓库查询异常时降级不失败) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 营养表缺失/数据库故障：findByNames 抛异常 → 投稿不失败，降级为回退形态
    EXPECT_CALL(*nutMock, findByNames(_))
        .WillOnce(Throw(gocook::services::ServiceException("数据库操作失败", 500)));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    // 无手填 → 空对象；不抛异常
    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    auto req = makeReq({makeIng("鸡胸肉", 100, "克")});
    service.submitRecipe(42, req);
    EXPECT_TRUE(captured.is_object());
    EXPECT_TRUE(captured.empty());
}

TEST(RecipeNutritionTest, 计数单位无单重则跳过) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 番茄无 default_portion_g → "2个" 无法换算 → 全部跳过 → 空对象
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("番茄", 20.0, 0.9, 0.2, 4.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("番茄", 2, "个")}));
    EXPECT_TRUE(captured.empty());
}

// ═══════════════════════════════════════════════════════════════
// 回退路径（未注入营养仓库）
// ═══════════════════════════════════════════════════════════════

TEST(RecipeNutritionTest, 未注入营养仓库回退用户手填) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    auto req = makeReq({makeIng("鸡胸肉", 100, "克")});
    Nutrition userNut;
    userNut.calories = 123; userNut.protein = 4; userNut.fat = 5; userNut.carbs = 6;
    req.nutrition = userNut;

    RecipeServiceImpl service(std::move(recipeMock));  // 不注入营养仓库
    service.submitRecipe(42, req);
    EXPECT_DOUBLE_EQ(captured["calories"].get<double>(), 123.0);
    EXPECT_DOUBLE_EQ(captured["protein"].get<double>(), 4.0);
    // 回退形态与"有仓库但全失败"路径一致：顶部四项与 per_serving 成对写入
    EXPECT_DOUBLE_EQ(captured["per_serving"]["calories"].get<double>(), 123.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["protein_g"].get<double>(), 4.0);
}

TEST(RecipeNutritionTest, 编辑时同样重算营养) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke(defaultMatcher));
    nlohmann::json captured;
    EXPECT_CALL(*repo, update(_, _, _, _)).WillOnce(Invoke(
        [&](int, int, const EditRecipeRequest&, const std::optional<nlohmann::json>& j) -> std::string {
            if (!j.has_value()) {
                ADD_FAILURE() << "正常计算路径应传入营养 JSON（非 nullopt）";
                return std::string("pending");
            }
            captured = *j;
            return std::string("pending");
        }));

    EditRecipeRequest updates;
    updates.name = "改名";
    updates.ingredients = {makeIng("鸡胸肉", 200, "克")};

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.editRecipe(42, 1, updates);
    expectNutritionJson(captured, 200.0, 20.0, 10.0, 20.0);
}

TEST(RecipeNutritionTest, 编辑时营养查询异常且无手填保留旧营养) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 营养表缺失/数据库故障：findByNames 抛异常、且编辑请求无手填值
    // → update 必须收到 nullopt（仓库 COALESCE 保留库中已有 nutrition_info，
    //   编辑不因计算不可用而清空已保存的营养数据）
    EXPECT_CALL(*nutMock, findByNames(_))
        .WillOnce(Throw(gocook::services::ServiceException("数据库操作失败", 500)));
    EXPECT_CALL(*repo, update(42, 1, _, testing::Eq(std::nullopt)))
        .WillOnce(Return(std::string("pending")));

    EditRecipeRequest updates;
    updates.name = "改名";
    updates.ingredients = {makeIng("鸡胸肉", 200, "克")};

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.editRecipe(42, 1, updates);
}

TEST(RecipeNutritionTest, 编辑时营养查询异常但有手填则回退手填) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 查询异常但编辑请求带手填值 → 以手填四项回退入库（用户显式补充优先于保留旧值）
    EXPECT_CALL(*nutMock, findByNames(_))
        .WillOnce(Throw(gocook::services::ServiceException("数据库操作失败", 500)));
    nlohmann::json captured;
    EXPECT_CALL(*repo, update(_, _, _, _)).WillOnce(Invoke(
        [&](int, int, const EditRecipeRequest&, const std::optional<nlohmann::json>& j) -> std::string {
            if (!j.has_value()) {
                ADD_FAILURE() << "有手填时应以手填四项回退（非 nullopt）";
                return std::string("pending");
            }
            captured = *j;
            return std::string("pending");
        }));

    EditRecipeRequest updates;
    updates.name = "改名";
    updates.ingredients = {makeIng("鸡胸肉", 200, "克")};
    Nutrition userNut;
    userNut.calories = 321; userNut.protein = 22; userNut.fat = 12; userNut.carbs = 33;
    updates.nutrition = userNut;

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.editRecipe(42, 1, updates);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["calories"].get<double>(), 321.0);
    EXPECT_DOUBLE_EQ(captured["per_serving"]["protein_g"].get<double>(), 22.0);
}

// ═══════════════════════════════════════════════════════════════
// 健康提示与营养报告透传
// ═══════════════════════════════════════════════════════════════

TEST(RecipeNutritionTest, 高钠菜谱生成健康提示) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto nutMock = std::make_unique<NiceMock<MockNutritionRepository>>();
    auto* repo = recipeMock.get();

    // 盐 10克：钠 38758mg/100g → 3875.8mg → 触发"钠含量较高"
    EXPECT_CALL(*nutMock, findByNames(_)).WillOnce(Invoke([](const std::vector<std::string>& names) {
        std::vector<std::optional<IngredientNutrition>> rows;
        for (const auto& n : names) rows.push_back(makeNut("盐", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 38758.0));
        return rows;
    }));
    EXPECT_CALL(*repo, existsByContent(_, _)).WillOnce(Return(false));
    nlohmann::json captured;
    EXPECT_CALL(*repo, create(_, _, _)).WillOnce(Invoke([&](int, const SubmitRecipeRequest&, const nlohmann::json& j) {
        captured = j;
        return SubmitRecipeResponse{1, "pending"};
    }));

    RecipeServiceImpl service(std::move(recipeMock), nullptr, nullptr, std::move(nutMock));
    service.submitRecipe(42, makeReq({makeIng("盐", 10, "克")}));
    EXPECT_THAT(captured["health_notes"].get<std::string>(), HasSubstr("钠含量较高"));
    EXPECT_DOUBLE_EQ(captured["per_serving"]["sodium_mg"].get<double>(), 3875.8);
}

TEST(RecipeNutritionTest, 高热量与高蛋白低脂健康提示) {
    // 100克：热量 800≥700 → 热量提示；蛋白 30≥25 且脂肪 10<15 → 高蛋白低脂提示
    auto j = submitSingle(makeNut("X", 800.0, 30.0, 10.0, 10.0), makeIng("X", 100, "克"));
    auto notes = j["health_notes"].get<std::string>();
    EXPECT_THAT(notes, HasSubstr("热量偏高"));
    EXPECT_THAT(notes, HasSubstr("高蛋白低脂"));
}

TEST(RecipeNutritionTest, 高蛋白高脂肪不触发低脂提示) {
    // 蛋白 30≥25 但脂肪 20≥15 → 不触发高蛋白低脂；其余阈值均未命中 → 空串
    auto j = submitSingle(makeNut("X", 300.0, 30.0, 20.0, 10.0), makeIng("X", 100, "克"));
    EXPECT_EQ(j["health_notes"].get<std::string>(), "");
}

TEST(RecipeNutritionTest, 高纤维健康提示) {
    auto j = submitSingle(makeNut("X", 100.0, 10.0, 5.0, 10.0, 0.0, 8.0), makeIng("X", 100, "克"));
    EXPECT_THAT(j["health_notes"].get<std::string>(), HasSubstr("膳食纤维"));
}

TEST(RecipeNutritionTest, 无健康提示命中时为空串) {
    auto j = submitSingle(makeNut("X", 100.0, 10.0, 5.0, 10.0, 0.0, 1.0, 100.0), makeIng("X", 100, "克"));
    EXPECT_EQ(j["health_notes"].get<std::string>(), "");
}

TEST(RecipeNutritionTest, 营养报告透传has_data) {
    auto recipeMock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = recipeMock.get();

    NutritionReport noData;
    noData.recipe_id = 7;
    noData.recipe_name = "无营养菜谱";
    noData.has_data = false;
    EXPECT_CALL(*repo, findNutrition(7)).WillOnce(Return(noData));

    RecipeServiceImpl service(std::move(recipeMock));
    auto report = service.getRecipeNutrition(7);
    EXPECT_FALSE(report.has_data);

    NutritionReport withData;
    withData.recipe_id = 7;
    withData.has_data = true;
    withData.per_serving.calories = 350.0;
    EXPECT_CALL(*repo, findNutrition(7)).WillOnce(Return(withData));
    report = service.getRecipeNutrition(7);
    EXPECT_TRUE(report.has_data);
}
