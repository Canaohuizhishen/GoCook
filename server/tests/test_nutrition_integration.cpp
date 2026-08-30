// test_nutrition_integration.cpp —— 营养相关 Repository 的集成测试（依赖真实 PostgreSQL）
//
// 覆盖审查缺口：
//   · PgIngredientNutritionRepository::findByNames（VALUES+LATERAL SQL）：
//     别名命中、规范名优先、未匹配 nullopt、按序对齐、重复入参去重、NULL 单重兜底
//   · PgRecipeRepository::findNutrition：含 per_serving 对象且七项（热量/蛋白/脂肪/碳水/
//     纤维/钠/维C）任一 >0 → has_data=true（纯钠调味料如"盐"也判有数据）、
//     空对象 {} / per_serving 空对象或七项全 0 → has_data=false、旧 flat-only 正数 → has_data=false、
//     SQL NULL → has_data=false、excluded_ingredients 透传
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致，非秘密），
// 可用环境变量 GOCOOK_TEST_DB 覆盖；探测失败即跳过（GTEST_SKIP）。
// 测试数据使用"集成测试"前缀，SetUp/TearDown 清理，不影响业务数据。

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>

#include "../common/ConnectionPool.h"
#include "../repositories/PgIngredientNutritionRepository.h"
#include "../repositories/PgRecipeRepository.h"

namespace {

const char* kDefaultTestConn =
    "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";

std::string testConnString() {
    const char* env = std::getenv("GOCOOK_TEST_DB");
    return env ? std::string(env) : std::string(kDefaultTestConn);
}

ConnectionPool& testPool() {
    static ConnectionPool pool(testConnString(), /*maxSize=*/1);
    return pool;
}

bool dbAvailable() {
    try {
        auto guard = testPool().getConnection(std::chrono::milliseconds(2000));
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("SELECT 1");
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

class NutritionDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        cleanup();
        seedNutritionRows();
    }

    void TearDown() override { cleanup(); }

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("DELETE FROM recipes WHERE name LIKE '集成测试菜谱%'");
        ntxn.exec("DELETE FROM ingredient_nutrition WHERE name LIKE '集成测试食%'");
        ntxn.exec("DELETE FROM users WHERE username = '集成测试用户'");
    }

    // 食A：有单重；食B：无单重，且别名"集成测试食A"与食A的规范名冲突（测规范名优先）
    // 注意：表 10 列，每行必须给足 10 个值（vitamin_c_mg NOT NULL，不能缺/不能 NULL）
    static void seedNutritionRows() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec(
            "INSERT INTO ingredient_nutrition"
            " (name, aliases, default_portion_g, calories, protein_g, fat_g,"
            "  carbs_g, fiber_g, sodium_mg, vitamin_c_mg) VALUES"
            " ('集成测试食A', ARRAY['集成测试别名A'], 100, 1, 2, 3, 4, 5, 6, 7),"
            " ('集成测试食B', ARRAY['集成测试食A'], NULL, 7, 8, 9, 10, 11, 12, 13)");
    }

    // 插入测试菜谱，返回 id；nutritionJson 为空串时 nutrition_info 为 SQL NULL。
    // 附空 tags（'{}'）——findById 的 array_to_json(tags) 在 NULL 时无默认值兜底，
    // 不带 tags 的行在详情查询时会抛"SQL null 转 string"异常
    static int insertRecipe(const std::string& nutritionJson) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        std::string name = "集成测试菜谱" + std::to_string(++s_recipeSeq);
        pqxx::result r;
        if (nutritionJson.empty()) {
            r = ntxn.exec(
                "INSERT INTO recipes (name, nutrition_info, tags) VALUES ($1, NULL, '{}') RETURNING id",
                pqxx::params{name});
        } else {
            r = ntxn.exec(
                "INSERT INTO recipes (name, nutrition_info, tags) VALUES ($1, $2::jsonb, '{}') RETURNING id",
                pqxx::params{name, nutritionJson});
        }
        return r[0][0].as<int>();
    }

    static int s_recipeSeq;
};

int NutritionDbTest::s_recipeSeq = 0;

// ---- PgIngredientNutritionRepository::findByNames ----

TEST_F(NutritionDbTest, 别名命中返回规范名) {
    PgIngredientNutritionRepository repo(testPool());
    auto rows = repo.findByNames({"集成测试别名A"});
    ASSERT_EQ(rows.size(), 1u);
    ASSERT_TRUE(rows[0].has_value());
    EXPECT_EQ(rows[0]->name, "集成测试食A");
    EXPECT_DOUBLE_EQ(rows[0]->default_portion_g, 100.0);
}

TEST_F(NutritionDbTest, 规范名优先于别名) {
    // "集成测试食A" 同时是 食B 的别名：必须命中规范名（食A），不能按别名返回 食B
    PgIngredientNutritionRepository repo(testPool());
    auto rows = repo.findByNames({"集成测试食A"});
    ASSERT_EQ(rows.size(), 1u);
    ASSERT_TRUE(rows[0].has_value());
    EXPECT_EQ(rows[0]->name, "集成测试食A");
}

TEST_F(NutritionDbTest, 未匹配为nullopt且按序对齐) {
    PgIngredientNutritionRepository repo(testPool());
    auto rows = repo.findByNames({"集成测试食A", "不存在食材XYZ", "集成测试别名A"});
    ASSERT_EQ(rows.size(), 3u);
    ASSERT_TRUE(rows[0].has_value());
    EXPECT_EQ(rows[0]->name, "集成测试食A");
    EXPECT_FALSE(rows[1].has_value());
    ASSERT_TRUE(rows[2].has_value());
    EXPECT_EQ(rows[2]->name, "集成测试食A");
}

TEST_F(NutritionDbTest, 重复入参每个至多一行) {
    PgIngredientNutritionRepository repo(testPool());
    auto rows = repo.findByNames({"集成测试别名A", "集成测试别名A"});
    ASSERT_EQ(rows.size(), 2u);
    ASSERT_TRUE(rows[0].has_value());
    ASSERT_TRUE(rows[1].has_value());
    EXPECT_EQ(rows[0]->name, rows[1]->name);
}

TEST_F(NutritionDbTest, NULL单重兜底为0) {
    PgIngredientNutritionRepository repo(testPool());
    auto rows = repo.findByNames({"集成测试食B"});
    ASSERT_EQ(rows.size(), 1u);
    ASSERT_TRUE(rows[0].has_value());
    EXPECT_DOUBLE_EQ(rows[0]->default_portion_g, 0.0);
}

// ---- PgRecipeRepository::findNutrition ----

TEST_F(NutritionDbTest, 空对象为无数据) {
    PgRecipeRepository repo(testPool());
    int id = insertRecipe("{}");
    EXPECT_FALSE(repo.findNutrition(id).has_data);
    EXPECT_FALSE(repo.findById(id).nutrition.has_data) << "空对象：详情页应显示空态";
}

TEST_F(NutritionDbTest, per_serving空对象为无数据) {
    PgRecipeRepository repo(testPool());
    int id = insertRecipe(R"({"per_serving":{}})");
    EXPECT_FALSE(repo.findNutrition(id).has_data);
    EXPECT_FALSE(repo.findById(id).nutrition.has_data) << "per_serving 空对象：详情页应显示空态";
}

TEST_F(NutritionDbTest, per_serving全0为无数据) {
    PgRecipeRepository repo(testPool());
    int id = insertRecipe(
        R"({"calories":0,"protein":0,"fat":0,"carbs":0,)"
        R"("per_serving":{"calories":0,"protein_g":0,"fat_g":0,"carbs_g":0,)"
        R"("fiber_g":0,"sodium_mg":0,"vitamin_c_mg":0}})");
    EXPECT_FALSE(repo.findNutrition(id).has_data);
    EXPECT_FALSE(repo.findById(id).nutrition.has_data) << "per_serving 全 0：详情页应显示空态";
}

TEST_F(NutritionDbTest, 宏量全0仅钠有值为有数据) {
    // 纯调味料菜谱（如"盐 10 克"）宏量四项全 0 但钠有真实值，属合法营养报告，
    // 不得按"全 0 占位"判为无数据（每 100g 盐钠 3875.8mg，10g 用量即上值）。
    PgRecipeRepository repo(testPool());
    int id = insertRecipe(
        R"({"calories":0,"protein":0,"fat":0,"carbs":0,)"
        R"("per_serving":{"calories":0,"protein_g":0,"fat_g":0,"carbs_g":0,)"
        R"("fiber_g":0,"sodium_mg":3875.8,"vitamin_c_mg":0}})");
    auto report = repo.findNutrition(id);
    EXPECT_TRUE(report.has_data);
    EXPECT_DOUBLE_EQ(report.per_serving.sodium_mg, 3875.8);
    // 详情页与报告页判定一致
    EXPECT_TRUE(repo.findById(id).nutrition.has_data) << "仅钠有值：详情页应显示营养数据";
}

TEST_F(NutritionDbTest, 旧flat格式正数为无数据) {
    // 判定收口回归：无 per_serving 的旧 flat-only 行即使四项均为正数，两页也统一判无数据
    // （前端展示空态而非按旧格式展示——该行为由判定收口引入，此处钉死）
    PgRecipeRepository repo(testPool());
    int id = insertRecipe(
        R"({"calories":350,"protein":12.5,"fat":8.2,"carbs":40.1})");
    EXPECT_FALSE(repo.findNutrition(id).has_data);
    EXPECT_FALSE(repo.findById(id).nutrition.has_data) << "旧 flat-only 正数：详情页应显示空态";
}

TEST_F(NutritionDbTest, NULL营养为无数据) {
    PgRecipeRepository repo(testPool());
    int id = insertRecipe("");
    auto report = repo.findNutrition(id);
    EXPECT_FALSE(report.has_data);
    EXPECT_DOUBLE_EQ(report.per_serving.calories, 0.0);
}

TEST_F(NutritionDbTest, 新格式透传excluded与per_serving) {
    PgRecipeRepository repo(testPool());
    std::string nut =
        R"({"calories":100,"protein":10,"fat":5,"carbs":20,)"
        R"("per_serving":{"calories":100,"protein_g":10,"fat_g":5,"carbs_g":20,)"
        R"("fiber_g":1,"sodium_mg":2,"vitamin_c_mg":3},)"
        R"("ingredients_breakdown":[{"name":"鸡","calories":100,"protein_g":10,"fat_g":5,"carbs_g":20}],)"
        R"("excluded_ingredients":[{"name":"盐","reason":"无法换算"},{"name":"秘料","reason":"未收录"}],)"
        R"("health_notes":"测试提示"})";
    int id = insertRecipe(nut);
    auto report = repo.findNutrition(id);
    EXPECT_TRUE(report.has_data);
    EXPECT_DOUBLE_EQ(report.per_serving.fiber_g, 1.0);
    EXPECT_DOUBLE_EQ(report.per_serving.sodium_mg, 2.0);
    ASSERT_EQ(report.ingredients_breakdown.size(), 1u);
    ASSERT_EQ(report.excluded_ingredients.size(), 2u);
    // 新格式：详情页 has_data 同样为 true（与报告页一致）
    EXPECT_TRUE(repo.findById(id).nutrition.has_data);
    EXPECT_EQ(report.excluded_ingredients[0].name, "盐");
    EXPECT_EQ(report.excluded_ingredients[0].reason, "无法换算");
    EXPECT_EQ(report.excluded_ingredients[1].name, "秘料");
    EXPECT_EQ(report.excluded_ingredients[1].reason, "未收录");
    EXPECT_EQ(report.health_notes, "测试提示");
}

// ---- PgRecipeRepository::update 的 nutrition_info 保留/覆盖语义 ----

TEST_F(NutritionDbTest, update传nullopt保留旧营养传空对象清空) {
    PgRecipeRepository repo(testPool());

    // 准备测试用户与菜谱（连接池 maxSize=1：必须先归还连接，再调 repo.update）
    int authorId = 0;
    int id = 0;
    {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        // 测试用户（不存在则创建）
        pqxx::result u = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ('集成测试用户', 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id");
        authorId = u[0][0].as<int>();

        // 带营养数据的菜谱（模拟"编辑前已保存过营养"）
        const std::string nut =
            R"({"calories":100,"protein":10,"fat":5,"carbs":20,)"
            R"("per_serving":{"calories":100,"protein_g":10,"fat_g":5,"carbs_g":20,)"
            R"("fiber_g":1,"sodium_mg":2,"vitamin_c_mg":3},)"
            R"("ingredients_breakdown":[],"excluded_ingredients":[],"health_notes":""})";
        std::string name = "集成测试菜谱" + std::to_string(++s_recipeSeq);
        pqxx::result r = ntxn.exec(
            "INSERT INTO recipes (name, author_id, nutrition_info) VALUES ($1, $2, $3::jsonb) RETURNING id",
            pqxx::params{name, authorId, nut});
        id = r[0][0].as<int>();
    }

    // 编辑请求（仅改名，食材/步骤等为空——营养计算不可用时的典型形态）
    gocook::models::EditRecipeRequest req;
    req.name = "集成测试菜谱改名";

    // 1) nutritionInfo = nullopt（营养表查询异常且无手填）→ 保留旧营养，不因编辑被清空
    EXPECT_EQ(repo.update(authorId, id, req, std::nullopt), "pending");
    auto kept = repo.findNutrition(id);
    ASSERT_TRUE(kept.has_data) << "nullopt 应保留库中已有 nutrition_info";
    EXPECT_DOUBLE_EQ(kept.per_serving.calories, 100.0);
    EXPECT_DOUBLE_EQ(kept.per_serving.fiber_g, 1.0);
    EXPECT_DOUBLE_EQ(kept.per_serving.sodium_mg, 2.0);

    // 2) nutritionInfo = {}（食材全部未收录的正常回退）→ 覆盖为空态
    EXPECT_EQ(repo.update(authorId, id, req, nlohmann::json::object()), "pending");
    EXPECT_FALSE(repo.findNutrition(id).has_data);
}
