// test_inventory_integration.cpp —— 库存单位语义重构（三维唯一约束 / POST 累加 / PUT 替换 / 勾选回流）的
// PgInventoryRepository / PgRecipeRepository 集成测试（依赖真实 PostgreSQL）
//
// 覆盖仓库层 SQL 行为测试缺口（此前只有 mock 委派测试，测不到 SQL 行为）：
//   · upsertInventory：同单位累加（返回原行 id、不带 expiry 不清既有到期日）、
//     同名不同单位各自成行互不覆盖、带 expiry 追加取行内最早到期日（含非补零日期输入）
//   · updateInventoryItem（PUT）：按 id 整行替换、缺省 expiry_date = 清空（审查修复）、
//     不存在/他人条目 404、改名改单位撞唯一约束 409（审查修复，行数据不变）
//   · updateShoppingListItem（勾选回流，api-spec 5.5）：同名同单位累加且不覆盖单位、
//     同名不同单位新建行且保留旧行
//   · PgRecipeRepository::findRecommendedRecipes：库存同名多单位行不撑大 match_count /
//     available_json（inv_names DISTINCT + inv_display 双 CTE）
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试"前缀 + 专用用户，
// SetUp/TearDown 清理，不影响业务数据。（模式同 test_nutrition_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../common/ConnectionPool.h"
#include "../repositories/PgInventoryRepository.h"
#include "../repositories/PgRecipeRepository.h"
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

class InventoryDbTest : public ::testing::Test {
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
        ntxn.exec("DELETE FROM recipes WHERE name LIKE '集成测试菜谱%'");
        ntxn.exec("DELETE FROM users WHERE username IN ('集成测试用户', '集成测试用户B')");
    }

    /// 创建（或复用）测试用户，返回用户 id
    static int ensureUser(const std::string& username = "集成测试用户") {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id",
            pqxx::params{username});
        return r[0][0].as<int>();
    }

    /// 直接经仓库添加一条库存（组装请求）
    static gocook::models::UpsertInventoryRequest makeReq(const std::string& name, double qty,
                                                          const std::string& unit,
                                                          const char* expiry = nullptr) {
        gocook::models::UpsertInventoryRequest req;
        req.ingredient_name = name;
        req.quantity = qty;
        req.unit = unit;
        if (expiry) req.expiry_date = std::string(expiry);
        return req;
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

    struct InvRow {
        int id = 0;
        double quantity = 0.0;
        std::string unit;
        std::optional<std::string> expiry_date;
    };

    /// 按 id 读库存行
    static std::optional<InvRow> fetchRow(int itemId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "SELECT id, quantity, unit, expiry_date FROM inventory WHERE id = $1",
            pqxx::params{itemId});
        if (r.empty()) return std::nullopt;
        InvRow row;
        row.id = r[0][0].as<int>();
        row.quantity = r[0][1].as<double>();
        row.unit = r[0][2].c_str();
        if (!r[0][3].is_null()) row.expiry_date = std::string(r[0][3].c_str());
        return row;
    }

    /// 按 用户+食材名 列出全部行（验证跨单位并存）
    static std::vector<InvRow> fetchByName(int userId, const std::string& name) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "SELECT id, quantity, unit, expiry_date FROM inventory"
            " WHERE user_id = $1 AND ingredient_name = $2 ORDER BY id",
            pqxx::params{userId, name});
        std::vector<InvRow> rows;
        for (const auto& row : r) {
            InvRow item;
            item.id = row[0].as<int>();
            item.quantity = row[1].as<double>();
            item.unit = row[2].c_str();
            if (!row[3].is_null()) item.expiry_date = std::string(row[3].c_str());
            rows.push_back(std::move(item));
        }
        return rows;
    }

    /// 造一条未勾选的购物清单项，返回 item id（回流前状态）
    static int insertUncheckedItem(int listId, const std::string& name, double required,
                                   double invQty, double toBuy, const std::string& unit) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO shopping_list_items"
            " (list_id, ingredient_name, required_quantity, inventory_quantity,"
            "  to_buy_quantity, unit, checked)"
            " VALUES ($1, $2, $3, $4, $5, $6, FALSE) RETURNING id",
            pqxx::params{listId, name, required, invQty, toBuy, unit});
        return r[0][0].as<int>();
    }

    static int insertRecipeWithIngredient(int authorId, const std::string& ingredientJson) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        std::string name = "集成测试菜谱" + std::to_string(++s_recipeSeq);
        pqxx::result r = ntxn.exec(
            "INSERT INTO recipes (name, author_id, ingredients, status, tags)"
            " VALUES ($1, $2, $3::jsonb, 'approved', '{}') RETURNING id",
            pqxx::params{name, authorId, ingredientJson});
        return r[0][0].as<int>();
    }

    static int s_recipeSeq;
};

int InventoryDbTest::s_recipeSeq = 0;

// ---- upsertInventory（POST /api/inventory 添加语义） ----

TEST_F(InventoryDbTest, 添加同单位累加且不带日期不清既有到期日) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id1 = repo.upsertInventory(uid, makeReq("集成测试鸡蛋", 2, "个", "2026-04-20"));
    // 同单位再次添加 = 又买一笔 → 命中同一行累加，返回原行 id
    int id2 = repo.upsertInventory(uid, makeReq("集成测试鸡蛋", 3, "个"));
    ASSERT_EQ(id1, id2);

    auto row = fetchRow(id1);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 5.0);
    // 不带 expiry 的追加不清既有到期日（POST 缺省 = 不清，与 PUT 缺省 = 清空分离）
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-04-20");
}

TEST_F(InventoryDbTest, 添加同名不同单位各自成行互不覆盖) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int idMl = repo.upsertInventory(uid, makeReq("集成测试料酒", 15, "毫升"));
    int idG  = repo.upsertInventory(uid, makeReq("集成测试料酒", 100, "克"));

    EXPECT_NE(idMl, idG);  // 不同单位 → 新行，不覆盖旧行（三维唯一约束落库后亦不冲突）
    auto rows = fetchByName(uid, "集成测试料酒");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].unit, "毫升");
    EXPECT_DOUBLE_EQ(rows[0].quantity, 15.0);
    EXPECT_EQ(rows[1].unit, "克");
    EXPECT_DOUBLE_EQ(rows[1].quantity, 100.0);
}

TEST_F(InventoryDbTest, 带过期日追加取行内最早到期日) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id = repo.upsertInventory(uid, makeReq("集成测试牛奶", 500, "毫升", "2026-04-12"));
    // 新批次（更晚到期）混入 → 到期日仍取最早（安全下限：混批无法分辨，按最早整行弃用）
    repo.upsertInventory(uid, makeReq("集成测试牛奶", 200, "毫升", "2026-04-20"));
    auto row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 700.0);
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-04-12");

    // 非补零格式输入（2026-3-5）也要按真实日期比较，不能被字典序骗过
    repo.upsertInventory(uid, makeReq("集成测试牛奶", 100, "毫升", "2026-3-5"));
    row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 800.0);
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-03-05");
}

TEST_F(InventoryDbTest, 原有到期日早于新批次则保留原到期日) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id = repo.upsertInventory(uid, makeReq("集成测试酸奶", 100, "克", "2026-06-01"));
    repo.upsertInventory(uid, makeReq("集成测试酸奶", 50, "克", "2026-05-01"));  // 更早的新批次
    auto row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 150.0);
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-05-01");
}

TEST_F(InventoryDbTest, 无过期日行追加带过期日则设置过期日) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id = repo.upsertInventory(uid, makeReq("集成测试火腿", 1, "根"));
    auto before = fetchRow(id);
    ASSERT_TRUE(before.has_value());
    EXPECT_FALSE(before->expiry_date.has_value());

    repo.upsertInventory(uid, makeReq("集成测试火腿", 2, "根", "2026-05-10"));
    auto row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-05-10");
}

// ---- updateInventoryItem（PUT /api/inventory/:id 编辑语义） ----

TEST_F(InventoryDbTest, 编辑按id整行替换改名改量改单位改日期) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id = repo.upsertInventory(uid, makeReq("集成测试料酒", 15, "毫升", "2026-05-01"));
    repo.updateInventoryItem(uid, id, makeReq("集成测试蚝油", 1, "瓶", "2026-06-01"));

    auto row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->unit, "瓶");
    EXPECT_DOUBLE_EQ(row->quantity, 1.0);
    ASSERT_TRUE(row->expiry_date.has_value());
    EXPECT_EQ(*row->expiry_date, "2026-06-01");
    // 原名字行被整体替换，不存在残留
    EXPECT_TRUE(fetchByName(uid, "集成测试料酒").empty());
}

TEST_F(InventoryDbTest, 编辑缺省过期日清空既有过期日) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int id = repo.upsertInventory(uid, makeReq("集成测试面粉", 1, "袋", "2026-07-01"));
    // 编辑弹窗里清空日期 → 请求缺省 expiry_date → 整行替换语义 = 置空
    repo.updateInventoryItem(uid, id, makeReq("集成测试面粉", 2, "袋"));

    auto row = fetchRow(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 2.0);
    EXPECT_FALSE(row->expiry_date.has_value()) << "PUT 缺省 expiry_date 必须清空旧日期";
}

TEST_F(InventoryDbTest, 编辑不存在条目返回404) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());
    expectError([&] { repo.updateInventoryItem(uid, 9999999, makeReq("集成测试料酒", 1, "毫升")); },
                404);
}

TEST_F(InventoryDbTest, 编辑他人条目返回404) {
    int uidA = ensureUser("集成测试用户");
    int uidB = ensureUser("集成测试用户B");
    PgInventoryRepository repo(testPool());

    int idB = repo.upsertInventory(uidB, makeReq("集成测试料酒", 15, "毫升"));
    // A 编辑 B 的条目 → 归属校验失败，404 而非越权更新
    expectError([&] { repo.updateInventoryItem(uidA, idB, makeReq("集成测试料酒", 99, "毫升")); },
                404);
    // B 的行未被改动
    auto row = fetchRow(idB);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 15.0);
}

TEST_F(InventoryDbTest, 编辑改名撞同名同单位唯一约束返回409且数据不变) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int idWine = repo.upsertInventory(uid, makeReq("集成测试料酒", 15, "毫升"));
    int idSauce = repo.upsertInventory(uid, makeReq("集成测试蚝油", 1, "瓶"));

    // 把 料酒/毫升 行改名为 蚝油/瓶 → 与已有行撞 (user, name, unit) → 409
    expectError([&] {
        repo.updateInventoryItem(uid, idWine, makeReq("集成测试蚝油", 1, "瓶"));
    }, 409, "同名同单位");

    // 冲突事务回滚，两行原样保留
    auto wine = fetchRow(idWine);
    auto sauce = fetchRow(idSauce);
    ASSERT_TRUE(wine.has_value());
    ASSERT_TRUE(sauce.has_value());
    EXPECT_EQ(wine->unit, "毫升");
    EXPECT_DOUBLE_EQ(wine->quantity, 15.0);
    EXPECT_EQ(sauce->unit, "瓶");
}

// ---- updateShoppingListItem（勾选回流，api-spec 5.5） ----

TEST_F(InventoryDbTest, 勾选回流同单位累加不覆盖单位) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int invId = repo.upsertInventory(uid, makeReq("集成测试牛奶", 500, "毫升"));
    gocook::models::CreateShoppingListRequest listReq;
    listReq.name = "集成测试清单";
    int listId = repo.createShoppingList(uid, listReq);
    int itemId = insertUncheckedItem(listId, "集成测试牛奶", 700, 500, 200, "毫升");

    gocook::models::UpdateShoppingItemRequest check;
    check.checked = true;
    repo.updateShoppingListItem(uid, listId, itemId, check);

    auto rows = fetchByName(uid, "集成测试牛奶");
    ASSERT_EQ(rows.size(), 1u) << "同单位回流应累加进原行，不新增";
    EXPECT_EQ(rows[0].id, invId);
    EXPECT_DOUBLE_EQ(rows[0].quantity, 700.0);
    EXPECT_EQ(rows[0].unit, "毫升");
}

TEST_F(InventoryDbTest, 勾选回流跨单位新建行保留旧行) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int oldId = repo.upsertInventory(uid, makeReq("集成测试面粉", 1, "袋"));
    gocook::models::CreateShoppingListRequest listReq;
    listReq.name = "集成测试清单";
    int listId = repo.createShoppingList(uid, listReq);
    // 清单按"克"买了 500 克，而库存里只有 1 袋 → 不许 500 + 1，也不许覆盖袋数
    int itemId = insertUncheckedItem(listId, "集成测试面粉", 500, 0, 500, "克");

    gocook::models::UpdateShoppingItemRequest check;
    check.checked = true;
    repo.updateShoppingListItem(uid, listId, itemId, check);

    auto rows = fetchByName(uid, "集成测试面粉");
    ASSERT_EQ(rows.size(), 2u);
    bool foundOld = false, foundNew = false;
    for (const auto& r : rows) {
        if (r.id == oldId) { foundOld = true; EXPECT_EQ(r.unit, "袋"); EXPECT_DOUBLE_EQ(r.quantity, 1.0); }
        if (r.unit == "克") { foundNew = true; EXPECT_DOUBLE_EQ(r.quantity, 500.0); }
    }
    EXPECT_TRUE(foundOld) << "旧行（袋）必须保留";
    EXPECT_TRUE(foundNew) << "新行（克）应新建";
}

TEST_F(InventoryDbTest, 取消勾选与重复勾选不会重复回流) {
    int uid = ensureUser();
    PgInventoryRepository repo(testPool());

    int invId = repo.upsertInventory(uid, makeReq("集成测试鸡蛋", 2, "个"));
    gocook::models::CreateShoppingListRequest listReq;
    listReq.name = "集成测试清单";
    int listId = repo.createShoppingList(uid, listReq);
    int itemId = insertUncheckedItem(listId, "集成测试鸡蛋", 10, 2, 8, "个");

    gocook::models::UpdateShoppingItemRequest req;
    req.checked = true;
    repo.updateShoppingListItem(uid, listId, itemId, req);   // false→true：回流一次
    repo.updateShoppingListItem(uid, listId, itemId, req);   // true→true：不回流
    req.checked = false;
    repo.updateShoppingListItem(uid, listId, itemId, req);   // true→false：不回流
    req.checked = true;
    repo.updateShoppingListItem(uid, listId, itemId, req);   // false→true：再次回流

    auto row = fetchRow(invId);
    ASSERT_TRUE(row.has_value());
    EXPECT_DOUBLE_EQ(row->quantity, 2.0 + 8.0 * 2) << "仅两次 false→true 触发回流，共 +16";
}

// ---- PgRecipeRepository::findRecommendedRecipes（同名多单位不撑大计数） ----

TEST_F(InventoryDbTest, 库存同名多单位行不撑大推荐计数与匹配明细) {
    int uid = ensureUser();
    PgInventoryRepository invRepo(testPool());
    PgRecipeRepository recipeRepo(testPool());

    // 库存：料酒 两行（15 毫升 + 100 克，三维约束允许并存）
    invRepo.upsertInventory(uid, makeReq("集成测试料酒", 15, "毫升"));
    invRepo.upsertInventory(uid, makeReq("集成测试料酒", 100, "克"));

    // 菜谱只需一种食材：料酒 10 毫升
    int recipeId = insertRecipeWithIngredient(
        uid, R"([{"name":"集成测试料酒","quantity":10,"unit":"毫升"}])");

    auto result = recipeRepo.findRecommendedRecipes(uid, 1, 100);

    const gocook::models::RecommendedRecipe* mine = nullptr;
    for (const auto& rec : result.data) {
        if (rec.id == recipeId) { mine = &rec; break; }
    }
    ASSERT_NE(mine, nullptr) << "测试菜谱应出现在推荐候选里（approved + 有食材）";
    // 若 inv_names 不去重：1 个食材 join 2 行库存 → available_json 两条、match_count=2 → match_score=2.0
    EXPECT_EQ(mine->match_status.available_ingredients.size(), 1u) << "DISTINCT 前 available 明细会翻倍";
    EXPECT_TRUE(mine->match_status.missing_ingredients.empty());
    EXPECT_DOUBLE_EQ(mine->match_score, 1.0) << "DISTINCT 前 match_score 会虚高到 2.0";
    // 明细展示取最近录入的一行（100 克后录入）——仅作展示，不作为匹配依据
    EXPECT_EQ(mine->match_status.available_ingredients[0].name, "集成测试料酒");
    EXPECT_DOUBLE_EQ(mine->match_status.available_ingredients[0].quantity, 100.0);
    EXPECT_EQ(mine->match_status.available_ingredients[0].unit, "克");
}
