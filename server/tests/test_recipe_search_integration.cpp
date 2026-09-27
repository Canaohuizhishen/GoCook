// test_recipe_search_integration.cpp —— 菜谱关键词搜索（PgRecipeRepository::searchRecipes）
// 集成测试（依赖真实 PostgreSQL）
//
// 覆盖缺口：此前 searchRecipes 只有 service 层 mock 委派测试（test_recipe_service.cpp），
// 测不到 SQL 行为。本文件锁定"关键词完全参数化"（ILIKE '%' || $n || '%'）后的语义：
//   · 菜名 / 描述 / 食材名（ingredients JSONB 数组）三路模糊匹配各命中对应菜谱
//   · 关键词含单引号按字面命中（值走参数，不再是字符串拼接）
//   · 用户输入里的 % 与 _ 保持既有 LIKE 通配符语义（与改造前逐字节一致，不做库存式转义）
//   · 分页 total 恒为命中总数、跨页数据不重不漏
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试搜索"前缀，SetUp/TearDown 清理，
// 不影响业务数据。（模式同 test_inventory_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <set>
#include <string>

#include "../common/ConnectionPool.h"
#include "../repositories/PgRecipeRepository.h"
#include <gocook/DataModels.h>

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

class RecipeSearchDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        cleanup();
    }

    void TearDown() override { cleanup(); }

    // 注意：连接池 maxSize=1，helper 内必须用完即还（作用域块），再调 repo 方法

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("DELETE FROM recipes WHERE name LIKE '集成测试搜索%'");
    }

    /// 直接插入一条 approved 测试菜谱（指定名称/描述/食材 JSON），返回菜谱 id
    static int insertRecipe(const std::string& name,
                            const std::string& description,
                            const std::string& ingredientsJson) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO recipes (name, description, ingredients, status, tags)"
            " VALUES ($1, $2, $3::jsonb, 'approved', '{}') RETURNING id",
            pqxx::params{name, description, ingredientsJson});
        return r[0][0].as<int>();
    }

    /// 命中结果里是否含指定 id（并返回该条，供进一步断言）
    static const gocook::models::RecipeSummary* containsId(
        const gocook::models::PagedRecipes& paged, int id) {
        for (const auto& rec : paged.data)
            if (rec.id == id) return &rec;
        return nullptr;
    }

    /// 直查 approved 总数（用于通配符全量命中的对照）
    static int countApproved() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        return ntxn.exec("SELECT COUNT(*) FROM recipes WHERE status = 'approved'")[0][0].as<int>();
    }
};

TEST_F(RecipeSearchDbTest, 关键词三路命中菜名描述与食材) {
    PgRecipeRepository repo(testPool());

    // 三个互不重叠的词，各只出现在对应的一路（名称统一带清理前缀）
    const std::string kwName = "集成测试搜索词名";
    const std::string kwDesc = "集成测试搜索词述";
    const std::string kwIng  = "集成测试搜索词材";

    int byName = insertRecipe(kwName, "普通描述", "[]");
    int byDesc = insertRecipe("集成测试搜索无关菜甲", "描述里藏着" + kwDesc, "[]");
    int byIng  = insertRecipe("集成测试搜索无关菜乙", "普通描述",
                              R"([{"name":"集成测试搜索词材","amount":"100克"}])");

    auto r1 = repo.searchRecipes(kwName, 1, 50, nlohmann::json::object());
    ASSERT_EQ(r1.pagination.total, 1) << "菜名路径应唯一命中";
    EXPECT_NE(containsId(r1, byName), nullptr);

    auto r2 = repo.searchRecipes(kwDesc, 1, 50, nlohmann::json::object());
    ASSERT_EQ(r2.pagination.total, 1) << "描述路径应唯一命中";
    EXPECT_NE(containsId(r2, byDesc), nullptr);

    auto r3 = repo.searchRecipes(kwIng, 1, 50, nlohmann::json::object());
    ASSERT_EQ(r3.pagination.total, 1) << "食材 JSONB 路径应唯一命中";
    EXPECT_NE(containsId(r3, byIng), nullptr);
}

TEST_F(RecipeSearchDbTest, 关键词含单引号按字面命中且不报错) {
    PgRecipeRepository repo(testPool());

    int id = insertRecipe("集成测试搜索'引号菜", "普通描述", "[]");

    // 参数化路径：单引号只作为值内容参与匹配；若回退成裸拼接会直接语法错误
    auto paged = repo.searchRecipes("集成测试搜索'引号", 1, 50, nlohmann::json::object());
    ASSERT_EQ(paged.pagination.total, 1);
    EXPECT_NE(containsId(paged, id), nullptr);
}

TEST_F(RecipeSearchDbTest, 百分号与下划线保持LIKE通配符语义) {
    PgRecipeRepository repo(testPool());

    insertRecipe("集成测试搜索兔子", "普通描述", "[]");
    insertRecipe("集成测试搜索乌龟", "普通描述", "[]");

    // 既有语义（改造前后一致）：pattern = '%' + 输入 + '%'，输入中的 % / _ 不转义，
    // 仍按 LIKE 通配符参与 —— 输入 "%" 等价于"匹配所有 approved 菜谱"
    int approved = countApproved();
    auto pct = repo.searchRecipes("%", 1, 50, nlohmann::json::object());
    EXPECT_EQ(pct.pagination.total, approved) << "% 按通配符全量命中（与改造前行为一致）";

    // "_" 匹配任意单字符（菜名非空）→ 同样全量命中
    auto und = repo.searchRecipes("_", 1, 50, nlohmann::json::object());
    EXPECT_EQ(und.pagination.total, approved) << "_ 按单字符通配符全量命中";

    // 注意：反斜杠是 PostgreSQL LIKE 的默认转义符（无 ESCAPE 子句时亦然），输入中的 "\"
    // 会按转义语义参与匹配；本用例仅锁定不报错（是否命中取决于数据与转义组合，不做断言）
    EXPECT_NO_THROW(repo.searchRecipes("\\", 1, 50, nlohmann::json::object()));
}

TEST_F(RecipeSearchDbTest, 分页total恒为命中总数且跨页数据一致) {
    PgRecipeRepository repo(testPool());

    int a = insertRecipe("集成测试搜索分页甲", "普通描述", "[]");
    int b = insertRecipe("集成测试搜索分页乙", "普通描述", "[]");
    int c = insertRecipe("集成测试搜索分页丙", "普通描述", "[]");

    auto page1 = repo.searchRecipes("集成测试搜索分页", 1, 2, nlohmann::json::object());
    auto page2 = repo.searchRecipes("集成测试搜索分页", 2, 2, nlohmann::json::object());

    ASSERT_EQ(page1.pagination.total, 3);
    ASSERT_EQ(page2.pagination.total, 3) << "total 与页码无关";
    ASSERT_EQ(page1.data.size(), 2u);
    ASSERT_EQ(page2.data.size(), 1u);

    std::set<int> ids;
    for (const auto& rec : page1.data) ids.insert(rec.id);
    for (const auto& rec : page2.data) ids.insert(rec.id);
    EXPECT_EQ(ids.size(), 3u) << "两页并集应为不重不漏的 3 条";
    EXPECT_TRUE(ids.count(a) > 0 && ids.count(b) > 0 && ids.count(c) > 0);
}
