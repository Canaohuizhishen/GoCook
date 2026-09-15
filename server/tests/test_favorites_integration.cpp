// test_favorites_integration.cpp —— 收藏批量更新 / 删除的 PgUserRepository 集成测试（依赖真实 PostgreSQL）
//
// 覆盖审查标记的 SQL 语义缺口（此前只有 mock 委派测试，测不到 SQL 行为）：
//   · batchUpdateFavorites：分组 + 可见性同批量更新（单事务逐条 UPDATE）
//   · group_id = 0 移至默认收藏夹 → group_id 置 NULL（NULL 语义保留）
//   · 不属于当前用户 / 不存在的收藏 id 静默跳过（与 3.6 批量删除宽松语义一致）
//   · 目标分组不属于当前用户 / 不存在 → 404「分组不存在」，且零行变更（先校验后写入）
//   · batchDeleteFavorites：本人收藏正常删除；他人 / 不存在的收藏 id 静默跳过（与 3.5 宽松语义一致）
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试"前缀 + 专用用户，
// SetUp/TearDown 清理，不影响业务数据。（模式同 test_inventory_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "../common/ConnectionPool.h"
#include "../repositories/PgUserRepository.h"
#include <gocook/DataModels.h>
#include <gocook/IServices.h>

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

class FavoritesDbTest : public ::testing::Test {
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
        // 先删菜谱（favorites.recipe_id 级联），再删用户（favorite_groups / favorites 级联）
        ntxn.exec("DELETE FROM recipes WHERE name LIKE '集成测试菜谱%'");
        ntxn.exec("DELETE FROM users WHERE username IN ('集成测试收藏用户A', '集成测试收藏用户B')");
    }

    /// 创建（或复用）测试用户，返回用户 id
    static int ensureUser(const std::string& username) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id",
            pqxx::params{username});
        return r[0][0].as<int>();
    }

    /// 创建收藏分组，返回分组 id
    static int insertGroup(int userId, const std::string& name) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO favorite_groups (user_id, name) VALUES ($1, $2) RETURNING id",
            pqxx::params{userId, name});
        return r[0][0].as<int>();
    }

    static int s_recipeSeq;

    /// 造一条测试菜谱，返回菜谱 id
    static int insertRecipe(int authorId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        std::string name = "集成测试菜谱收藏" + std::to_string(++s_recipeSeq);
        pqxx::result r = ntxn.exec(
            "INSERT INTO recipes (name, author_id, ingredients, status, tags)"
            " VALUES ($1, $2, '[]'::jsonb, 'approved', '{}') RETURNING id",
            pqxx::params{name, authorId});
        return r[0][0].as<int>();
    }

    /// 造一条收藏（groupId 为 nullopt = 默认收藏夹 / NULL），返回收藏 id
    static int insertFavorite(int userId, int recipeId, std::optional<int> groupId, bool isPublic) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r;
        if (groupId.has_value()) {
            r = ntxn.exec(
                "INSERT INTO favorites (user_id, recipe_id, group_id, is_public)"
                " VALUES ($1, $2, $3, $4) RETURNING id",
                pqxx::params{userId, recipeId, groupId.value(), isPublic});
        } else {
            r = ntxn.exec(
                "INSERT INTO favorites (user_id, recipe_id, is_public)"
                " VALUES ($1, $2, $3) RETURNING id",
                pqxx::params{userId, recipeId, isPublic});
        }
        return r[0][0].as<int>();
    }

    struct FavRow {
        int id = 0;
        int user_id = 0;
        std::optional<int> group_id;
        bool is_public = false;
    };

    /// 按收藏 id 读行状态
    static std::optional<FavRow> fetchFavorite(int favId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "SELECT id, user_id, group_id, is_public FROM favorites WHERE id = $1",
            pqxx::params{favId});
        if (r.empty()) return std::nullopt;
        FavRow row;
        row.id = r[0][0].as<int>();
        row.user_id = r[0][1].as<int>();
        if (!r[0][2].is_null()) row.group_id = r[0][2].as<int>();
        row.is_public = r[0][3].as<bool>();
        return row;
    }

    /// 断言调用抛 ServiceException(status)（可选校验消息包含 msgPart）
    template <typename Fn>
    static void expectError(Fn&& fn, int status, const std::string& msgPart = {}) {
        bool thrown = false;
        try {
            fn();
        } catch (const gocook::services::ServiceException& e) {
            thrown = true;
            EXPECT_EQ(e.statusCode(), status) << "异常消息: " << e.what();
            if (!msgPart.empty())
                EXPECT_NE(std::string(e.what()).find(msgPart), std::string::npos)
                    << "异常消息: " << e.what();
        }
        if (!thrown) FAIL() << "期望 ServiceException(" << status << ") 但未抛出";
    }
};

int FavoritesDbTest::s_recipeSeq = 0;

// ---- batchUpdateFavorites（PATCH /api/users/me/favorites/batch） ----

TEST_F(FavoritesDbTest, 批量移动分组并同时更新可见性) {
    int uid = ensureUser("集成测试收藏用户A");
    int groupId = insertGroup(uid, "集成测试分组");
    int fav1 = insertFavorite(uid, insertRecipe(uid), std::nullopt, true);
    int fav2 = insertFavorite(uid, insertRecipe(uid), groupId, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {fav1, fav2};
    req.group_id = groupId;
    req.is_public = false;
    repo.batchUpdateFavorites(uid, req);

    auto row1 = fetchFavorite(fav1);
    auto row2 = fetchFavorite(fav2);
    ASSERT_TRUE(row1.has_value());
    ASSERT_TRUE(row2.has_value());
    EXPECT_EQ(row1->group_id, groupId);   // NULL → 目标分组
    EXPECT_EQ(row2->group_id, groupId);   // 分组内不动
    EXPECT_FALSE(row1->is_public);        // 可见性同批量更新
    EXPECT_FALSE(row2->is_public);
}

TEST_F(FavoritesDbTest, 批量移动group_id为零置NULL即默认收藏夹) {
    int uid = ensureUser("集成测试收藏用户A");
    int groupId = insertGroup(uid, "集成测试分组");
    int fav1 = insertFavorite(uid, insertRecipe(uid), groupId, true);
    int fav2 = insertFavorite(uid, insertRecipe(uid), groupId, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {fav1, fav2};
    req.group_id = 0;                     // 0 = 移至默认收藏夹（NULL）
    repo.batchUpdateFavorites(uid, req);

    auto row1 = fetchFavorite(fav1);
    auto row2 = fetchFavorite(fav2);
    ASSERT_TRUE(row1.has_value());
    ASSERT_TRUE(row2.has_value());
    EXPECT_FALSE(row1->group_id.has_value());
    EXPECT_FALSE(row2->group_id.has_value());
    EXPECT_TRUE(row1->is_public);         // 未传 is_public 不改动
}

TEST_F(FavoritesDbTest, 批量更新仅切换可见性不动分组) {
    int uid = ensureUser("集成测试收藏用户A");
    int groupId = insertGroup(uid, "集成测试分组");
    int fav = insertFavorite(uid, insertRecipe(uid), groupId, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {fav};
    req.is_public = false;
    repo.batchUpdateFavorites(uid, req);

    auto row = fetchFavorite(fav);
    ASSERT_TRUE(row.has_value());
    EXPECT_FALSE(row->is_public);
    EXPECT_EQ(row->group_id, groupId);    // 分组不动
}

TEST_F(FavoritesDbTest, 批量更新静默跳过他人与不存在的收藏) {
    int uidA = ensureUser("集成测试收藏用户A");
    int uidB = ensureUser("集成测试收藏用户B");
    int groupA = insertGroup(uidA, "集成测试分组");
    int groupB = insertGroup(uidB, "集成测试分组B");
    int favA = insertFavorite(uidA, insertRecipe(uidA), std::nullopt, true);
    int favB = insertFavorite(uidB, insertRecipe(uidB), groupB, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {favA, favB, 99999999};   // 含他人收藏与不存在的 id
    req.group_id = groupA;
    req.is_public = false;                        // 若越权写他人收藏，可被下方断言捕获
    EXPECT_NO_THROW(repo.batchUpdateFavorites(uidA, req));

    auto rowA = fetchFavorite(favA);
    auto rowB = fetchFavorite(favB);
    ASSERT_TRUE(rowA.has_value());
    ASSERT_TRUE(rowB.has_value());
    EXPECT_EQ(rowA->group_id, groupA);            // A 的收藏正常更新
    EXPECT_EQ(rowB->group_id, groupB);            // B 的收藏分毫未动
    EXPECT_TRUE(rowB->is_public);
    EXPECT_EQ(rowB->user_id, uidB);
}

TEST_F(FavoritesDbTest, 目标分组不属于当前用户返回404且零变更) {
    int uidA = ensureUser("集成测试收藏用户A");
    int uidB = ensureUser("集成测试收藏用户B");
    int groupB = insertGroup(uidB, "集成测试分组B");
    int fav1 = insertFavorite(uidA, insertRecipe(uidA), std::nullopt, true);
    int fav2 = insertFavorite(uidA, insertRecipe(uidA), std::nullopt, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {fav1, fav2};
    req.group_id = groupB;                        // 他人分组
    expectError([&] { repo.batchUpdateFavorites(uidA, req); }, 404, "分组不存在");

    auto row1 = fetchFavorite(fav1);
    auto row2 = fetchFavorite(fav2);
    ASSERT_TRUE(row1.has_value());
    ASSERT_TRUE(row2.has_value());
    EXPECT_FALSE(row1->group_id.has_value());     // 校验前置：零行变更
    EXPECT_FALSE(row2->group_id.has_value());
    EXPECT_TRUE(row1->is_public);
}

TEST_F(FavoritesDbTest, 目标分组不存在返回404) {
    int uid = ensureUser("集成测试收藏用户A");
    int fav = insertFavorite(uid, insertRecipe(uid), std::nullopt, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchUpdateFavoritesRequest req;
    req.favorite_ids = {fav};
    req.group_id = 99999999;
    expectError([&] { repo.batchUpdateFavorites(uid, req); }, 404, "分组不存在");

    auto row = fetchFavorite(fav);
    ASSERT_TRUE(row.has_value());
    EXPECT_FALSE(row->group_id.has_value());
}

// ---- batchDeleteFavorites（POST/DELETE /api/users/me/favorites/batch） ----

TEST_F(FavoritesDbTest, 批量删除静默跳过他人与不存在的收藏) {
    int uidA = ensureUser("集成测试收藏用户A");
    int uidB = ensureUser("集成测试收藏用户B");
    int groupA = insertGroup(uidA, "集成测试分组");
    int favA1 = insertFavorite(uidA, insertRecipe(uidA), std::nullopt, true);
    int favA2 = insertFavorite(uidA, insertRecipe(uidA), groupA, true);
    int favB = insertFavorite(uidB, insertRecipe(uidB), std::nullopt, true);

    PgUserRepository repo(testPool());
    gocook::models::BatchDeleteFavoritesRequest req;
    req.favorite_ids = {favA1, favA2, favB, 99999999};   // 含他人收藏与不存在的 id
    EXPECT_NO_THROW(repo.batchDeleteFavorites(uidA, req));

    EXPECT_FALSE(fetchFavorite(favA1).has_value());      // A 的两条（默认夹 + 分组）均已删除
    EXPECT_FALSE(fetchFavorite(favA2).has_value());
    auto rowB = fetchFavorite(favB);
    ASSERT_TRUE(rowB.has_value());                       // B 的收藏分毫未动
    EXPECT_EQ(rowB->user_id, uidB);
}
