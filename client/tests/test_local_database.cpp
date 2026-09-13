#include <gtest/gtest.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantMap>
#include <QFile>
#include <QDir>
#include <QThread>
#include "LocalDatabase.h"

namespace {

// ==================== 测试夹具 ====================

/// 每个用例用独立的内存库 + 唯一连接名，互不干扰
class LocalDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        connName = QStringLiteral("gocook_test_%1").arg(s_counter++);
        db = LocalDatabase::createForTesting(QStringLiteral(":memory:"), connName);
        ASSERT_TRUE(db != nullptr);
        ASSERT_TRUE(db->isOpen());
    }

    void TearDown() override {
        delete db;
        db = nullptr;
        // 连接必须在其所有引用析构后（db 已删除）才能移除，否则 Qt 告警
        QSqlDatabase::removeDatabase(connName);
    }

    LocalDatabase* db = nullptr;
    QString connName;
    static int s_counter;
};

int LocalDatabaseTest::s_counter = 0;

// ==================== 菜谱详情缓存（断网兜底） ====================

TEST_F(LocalDatabaseTest, 详情缓存存取往返) {
    QVariantMap detail;
    detail["id"] = 7;
    detail["name"] = "红烧肉";
    detail["imageUrl"] = "/uploads/x.jpg";
    QVariantList steps;
    steps << QVariantMap{{"order", 1}, {"description", "切肉"}};
    detail["steps"] = steps;

    EXPECT_TRUE(db->saveRecipeDetailCache(7, detail));
    QVariantMap cached = db->getRecipeDetailCache(7);
    EXPECT_EQ(cached["name"].toString(), "红烧肉");
    EXPECT_EQ(cached["imageUrl"].toString(), "/uploads/x.jpg");
    EXPECT_EQ(cached["steps"].toList().size(), 1);
}

TEST_F(LocalDatabaseTest, 详情缓存按id区分) {
    db->saveRecipeDetailCache(1, QVariantMap{{"id", 1}, {"name", "A"}});
    db->saveRecipeDetailCache(2, QVariantMap{{"id", 2}, {"name", "B"}});
    EXPECT_EQ(db->getRecipeDetailCache(1)["name"].toString(), "A");
    EXPECT_EQ(db->getRecipeDetailCache(2)["name"].toString(), "B");
    EXPECT_TRUE(db->getRecipeDetailCache(99).isEmpty());
}

TEST_F(LocalDatabaseTest, 详情缓存同id覆盖更新) {
    db->saveRecipeDetailCache(1, QVariantMap{{"id", 1}, {"name", "旧"}});
    db->saveRecipeDetailCache(1, QVariantMap{{"id", 1}, {"name", "新"}});
    EXPECT_EQ(db->getRecipeDetailCache(1)["name"].toString(), "新");
    // upsert 不产生重复行
    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("SELECT COUNT(*) FROM recipe_detail_cache"));
    q.next();
    EXPECT_EQ(q.value(0).toInt(), 1);
}

// ==================== 详情缓存上限裁剪 ====================

// 时间戳为毫秒级（strftime %f）：间隔 2ms 写入保证 updated_at 严格递增，
// 否则同一毫秒内多条记录排序不确定，裁剪结果可能 flaky
TEST_F(LocalDatabaseTest, 详情缓存上限裁剪保留最近20条) {
    // 插入 25 条 → 最旧的 1-5 应被裁剪
    for (int i = 1; i <= 25; ++i) {
        QVariantMap detail;
        detail["id"] = i;
        detail["name"] = QString("菜谱%1").arg(i);
        ASSERT_TRUE(db->saveRecipeDetailCache(i, detail));
        QThread::msleep(2); // 保证时间戳严格递增（毫秒级）
    }
    for (int i = 1; i <= 5; ++i)
        EXPECT_TRUE(db->getRecipeDetailCache(i).isEmpty()) << "id=" << i << " 应已被裁剪";
    for (int i = 6; i <= 25; ++i)
        EXPECT_FALSE(db->getRecipeDetailCache(i).isEmpty()) << "id=" << i << " 应保留";
}

TEST_F(LocalDatabaseTest, 详情缓存重新访问刷新recency不被裁剪) {
    // 先写满 25 条（保留 6-25），再重新访问 id=1（时间戳刷新为最新），
    // 继续写入 id=26 → 裁剪应淘汰最旧的 6、7，而非刚刷新的 1
    for (int i = 1; i <= 25; ++i) {
        QVariantMap detail;
        detail["id"] = i;
        detail["name"] = QString("菜谱%1").arg(i);
        ASSERT_TRUE(db->saveRecipeDetailCache(i, detail));
        QThread::msleep(2);
    }
    // 重新访问 id=1（upsert 刷新 updated_at）
    ASSERT_TRUE(db->saveRecipeDetailCache(1, QVariantMap{{"id", 1}, {"name", "重新访问"}}));
    QThread::msleep(2);
    // 再写入一条，触发下一次裁剪：此时 20 条上限内应保留 1，淘汰 6、7
    ASSERT_TRUE(db->saveRecipeDetailCache(26, QVariantMap{{"id", 26}, {"name", "菜谱26"}}));

    EXPECT_FALSE(db->getRecipeDetailCache(1).isEmpty()) << "重新访问的 id=1 应保留";
    EXPECT_EQ(db->getRecipeDetailCache(1)["name"].toString(), "重新访问");
    EXPECT_TRUE(db->getRecipeDetailCache(6).isEmpty()) << "最旧的 id=6 应被裁剪";
    EXPECT_TRUE(db->getRecipeDetailCache(7).isEmpty()) << "最旧的 id=7 应被裁剪";
    EXPECT_FALSE(db->getRecipeDetailCache(26).isEmpty()) << "最新写入的 id=26 应保留";

    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("SELECT COUNT(*) FROM recipe_detail_cache"));
    q.next();
    EXPECT_EQ(q.value(0).toInt(), 20) << "裁剪后应恰好 20 条";
}

// ==================== 库存快照（在线优先 + 断网兜底） ====================

TEST_F(LocalDatabaseTest, 库存快照存取往返保留顺序与复合键) {
    ASSERT_TRUE(db->saveUser(1, "u", "t"));
    QVariantList snapshot;
    snapshot << QVariantMap{{"id", 7}, {"ingredientName", "盐"}, {"quantity", 5.0},
                            {"unit", "克"}, {"addedAt", "2026-05-01T00:00:00Z"}};
    snapshot << QVariantMap{{"id", 8}, {"ingredientName", "盐"}, {"quantity", 2.0},
                            {"unit", "茶匙"}, {"addedAt", "2026-05-02T00:00:00Z"}};
    snapshot << QVariantMap{{"id", 9}, {"ingredientName", "番茄"}, {"quantity", 3.0},
                            {"unit", "个"}, {"expiryDate", "2026-05-10"},
                            {"addedAt", "2026-05-03T00:00:00Z"}};
    ASSERT_TRUE(db->saveInventoryCache(snapshot));
    EXPECT_TRUE(db->hasInventoryCache());

    const QVariantList cached = db->getInventoryCache();
    ASSERT_EQ(cached.size(), 3);
    // 同名不同单位 = 两条（复合主键，对齐服务端三维语义）；顺序按写入序保留（ordinal）
    EXPECT_EQ(cached[0].toMap()["ingredientName"].toString(), "盐");
    EXPECT_EQ(cached[0].toMap()["unit"].toString(), "克");
    EXPECT_EQ(cached[1].toMap()["unit"].toString(), "茶匙");
    EXPECT_EQ(cached[2].toMap()["ingredientName"].toString(), "番茄");
    EXPECT_EQ(cached[2].toMap()["expiryDate"].toString(), "2026-05-10");
    EXPECT_DOUBLE_EQ(cached[2].toMap()["quantity"].toDouble(), 3.0);

    // 整体替换语义：再次保存只保留新快照
    QVariantList smaller;
    smaller << QVariantMap{{"id", 1}, {"ingredientName", "米"}, {"quantity", 1.0}, {"unit", "克"}};
    ASSERT_TRUE(db->saveInventoryCache(smaller));
    const QVariantList after = db->getInventoryCache();
    ASSERT_EQ(after.size(), 1);
    EXPECT_EQ(after[0].toMap()["ingredientName"].toString(), "米");
}

TEST_F(LocalDatabaseTest, 库存快照用户隔离与登出清理) {
    // 用户 A 写入快照
    ASSERT_TRUE(db->saveUser(100, "A", "tokenA"));
    QVariantList listA;
    listA << QVariantMap{{"ingredientName", "番茄"}, {"quantity", 3.0}, {"unit", "个"}};
    ASSERT_TRUE(db->saveInventoryCache(listA));
    EXPECT_TRUE(db->hasInventoryCache());

    // 直接换号（不登出）——读侧按当前用户过滤，B 不得命中 A 的快照；
    // 且 saveUser 物理清理非当前用户的快照行/标记（不依赖后续 clearUser）
    ASSERT_TRUE(db->saveUser(200, "B", "tokenB"));
    EXPECT_FALSE(db->hasInventoryCache()) << "换号后不得命中上一账号快照";
    EXPECT_TRUE(db->getInventoryCache().isEmpty()) << "快照必须按当前用户隔离";
    QSqlQuery raw(QSqlDatabase::database(connName));
    ASSERT_TRUE(raw.exec("SELECT COUNT(*) FROM inventory_cache"));
    raw.next();
    EXPECT_EQ(raw.value(0).toInt(), 0) << "换号后旧账号快照行必须被物理清理";
    ASSERT_TRUE(raw.exec("SELECT COUNT(*) FROM inventory_cache_meta"));
    raw.next();
    EXPECT_EQ(raw.value(0).toInt(), 0) << "换号后旧账号快照标记必须被物理清理";

    // 登出（clearUser）连带清理快照：再以 A 登录也不应看到旧快照
    ASSERT_TRUE(db->clearUser());
    ASSERT_TRUE(db->saveUser(100, "A", "tokenA"));
    EXPECT_FALSE(db->hasInventoryCache()) << "登出必须连带清理快照";
    EXPECT_TRUE(db->getInventoryCache().isEmpty());
}

TEST_F(LocalDatabaseTest, 空快照与无快照区分) {
    ASSERT_TRUE(db->saveUser(1, "u", "t"));
    EXPECT_FALSE(db->hasInventoryCache()) << "从未同步：无快照";

    ASSERT_TRUE(db->saveInventoryCache(QVariantList{}));
    EXPECT_TRUE(db->hasInventoryCache()) << "空快照（库存本来就为空）必须可区分于无快照";
    EXPECT_TRUE(db->getInventoryCache().isEmpty());
}

TEST_F(LocalDatabaseTest, 未登录拒绝写库存快照) {
    EXPECT_FALSE(db->saveInventoryCache(QVariantList{}));
    EXPECT_FALSE(db->hasInventoryCache());
    EXPECT_TRUE(db->getInventoryCache().isEmpty());
}

TEST_F(LocalDatabaseTest, 整批脏条目放弃写入不落空快照) {
    // 先落一份正常快照（旧数据）
    ASSERT_TRUE(db->saveUser(1, "u", "t"));
    QVariantList good;
    good << QVariantMap{{"ingredientName", "盐"}, {"quantity", 1.0}, {"unit", "克"}};
    ASSERT_TRUE(db->saveInventoryCache(good));

    // 整批缺键（契约外脏数据）→ 整体放弃：回滚保留旧快照与标记，不得落“空快照”
    // （否则离线兜底会把不可信数据谎报成“暂无库存”）
    QVariantList dirty;
    dirty << QVariantMap{{"quantity", 1.0}, {"unit", "克"}};           // 缺 ingredientName
    dirty << QVariantMap{{"ingredientName", "糖"}, {"quantity", 2.0}}; // 缺 unit
    EXPECT_FALSE(db->saveInventoryCache(dirty));
    EXPECT_TRUE(db->hasInventoryCache());
    const QVariantList kept = db->getInventoryCache();
    ASSERT_EQ(kept.size(), 1) << "整批脏数据必须回滚保留旧快照";
    EXPECT_EQ(kept[0].toMap()["ingredientName"].toString(), "盐");

    // 无旧快照 + 整批脏 → 不产生快照（离线走离线视图；空快照语义只留给真实空库存）
    ASSERT_TRUE(db->clearUser());
    ASSERT_TRUE(db->saveUser(2, "v", "t2"));
    EXPECT_FALSE(db->saveInventoryCache(dirty));
    EXPECT_FALSE(db->hasInventoryCache());

    // 部分脏：好行照常写入（尽力而为），不算整批失败
    QVariantList mixed;
    mixed << QVariantMap{{"ingredientName", "米"}, {"quantity", 3.0}, {"unit", "克"}};
    mixed << QVariantMap{{"quantity", 4.0}};
    ASSERT_TRUE(db->saveInventoryCache(mixed));
    EXPECT_TRUE(db->hasInventoryCache());
    ASSERT_EQ(db->getInventoryCache().size(), 1);
    EXPECT_EQ(db->getInventoryCache()[0].toMap()["ingredientName"].toString(), "米");
}

TEST_F(LocalDatabaseTest, 快照行级删除按复合键精确移除)
{
    ASSERT_TRUE(db->saveUser(1, "u", "t"));
    QVariantList snapshot;
    snapshot << QVariantMap{{"id", 7}, {"ingredientName", "盐"}, {"quantity", 5.0}, {"unit", "克"}};
    snapshot << QVariantMap{{"id", 8}, {"ingredientName", "盐"}, {"quantity", 2.0}, {"unit", "茶匙"}};
    snapshot << QVariantMap{{"id", 9}, {"ingredientName", "番茄"}, {"quantity", 3.0}, {"unit", "个"}};
    ASSERT_TRUE(db->saveInventoryCache(snapshot));

    // 同名不同单位：只删命中复合键的那一行，其余保留且顺序不变
    ASSERT_TRUE(db->removeInventoryCacheItem("盐", "克"));
    QVariantList left = db->getInventoryCache();
    ASSERT_EQ(left.size(), 2);
    EXPECT_EQ(left[0].toMap()["unit"].toString(), "茶匙");
    EXPECT_EQ(left[1].toMap()["ingredientName"].toString(), "番茄");

    // 行不存在：exec 成功即返回 true（no-op，不误删其它行）
    EXPECT_TRUE(db->removeInventoryCacheItem("不存在", "克"));
    EXPECT_EQ(db->getInventoryCache().size(), 2);

    // 空键拒绝（复合键不完整无法定位）
    EXPECT_FALSE(db->removeInventoryCacheItem("", "克"));
    EXPECT_FALSE(db->removeInventoryCacheItem("盐", ""));
    EXPECT_EQ(db->getInventoryCache().size(), 2);

    // 全部删光：meta 保留 → 空快照语义（区分于从未同步）
    ASSERT_TRUE(db->removeInventoryCacheItem("盐", "茶匙"));
    ASSERT_TRUE(db->removeInventoryCacheItem("番茄", "个"));
    EXPECT_TRUE(db->hasInventoryCache());
    EXPECT_TRUE(db->getInventoryCache().isEmpty());
}

TEST_F(LocalDatabaseTest, 未登录拒绝移除快照行)
{
    EXPECT_FALSE(db->removeInventoryCacheItem("盐", "克"));
}

// ==================== user 表（单行不变量） ====================

TEST_F(LocalDatabaseTest, 用户表单行不变量) {
    ASSERT_TRUE(db->saveUser(1, "A", "tokenA"));
    ASSERT_TRUE(db->saveUser(2, "B", "tokenB")); // 直接覆盖保存（不等登出）
    const QVariantMap user = db->getUser();
    EXPECT_EQ(user["id"].toInt(), 2);
    EXPECT_EQ(user["username"].toString(), "B");

    // 物理上只允许一行（防 getUser LIMIT 1 歧义）
    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("SELECT COUNT(*) FROM user"));
    q.next();
    EXPECT_EQ(q.value(0).toInt(), 1);

    ASSERT_TRUE(db->clearUser());
    EXPECT_TRUE(db->getUser().isEmpty());
}

// ==================== schema 版本门控（PRAGMA user_version） ====================

TEST_F(LocalDatabaseTest, 新建库直接为当前结构版本) {
    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("PRAGMA user_version"));
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 2) << "新建库应直接落当前 schema 版本";

    // 新结构列可用（复合主键键列 + ordinal）
    ASSERT_TRUE(q.exec("SELECT user_id, ingredient_name, unit, ordinal, data FROM inventory_cache LIMIT 1"));
}

TEST_F(LocalDatabaseTest, 存量v0库升级重建库存表且不丢其它数据) {
    // 手工造 v0 文件库：旧版单主键 inventory_cache + user 行 + 一条详情缓存（user_version 默认 0）
    const QString v0Conn = connName + "_v0";
    const QString v0Path = QDir::tempPath() + QStringLiteral("/gocook_v0_%1.db").arg(s_counter);
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase("QSQLITE", v0Conn);
        oldDb.setDatabaseName(v0Path);
        ASSERT_TRUE(oldDb.open());
        QSqlQuery q(oldDb);
        ASSERT_TRUE(q.exec("CREATE TABLE user (id INTEGER PRIMARY KEY, username TEXT, token TEXT)"));
        ASSERT_TRUE(q.exec("INSERT INTO user (id, username, token) VALUES (9, 'old', 'oldtoken')"));
        ASSERT_TRUE(q.exec("CREATE TABLE inventory_cache ("
                           "ingredient_name TEXT PRIMARY KEY, quantity REAL, unit TEXT, "
                           "expiry_date TEXT, added_at TEXT)"));
        ASSERT_TRUE(q.exec("CREATE TABLE recipe_detail_cache ("
                           "id INTEGER PRIMARY KEY, data TEXT NOT NULL, updated_at TEXT)"));
        ASSERT_TRUE(q.exec("INSERT INTO recipe_detail_cache (id, data, updated_at) "
                           "VALUES (5, '{\"id\":5,\"name\":\"旧缓存\"}', '2026-01-01 00:00:00.000')"));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(v0Conn);

    // 打开 → v0→v1 门控重建库存表
    delete db;
    QSqlDatabase::removeDatabase(connName); // 先注销 :memory: 连接，避免重复连接名告警
    db = LocalDatabase::createForTesting(v0Path, connName);
    ASSERT_TRUE(db->isOpen());

    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("PRAGMA user_version"));
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 2) << "打开后版本必须推进到 2（v0→v1→v2 步进）";

    // 库存表已是新结构且可用（复合主键）
    QVariantList snapshot;
    snapshot << QVariantMap{{"ingredientName", "新"}, {"quantity", 1.0}, {"unit", "克"}};
    EXPECT_TRUE(db->saveInventoryCache(snapshot));
    EXPECT_EQ(db->getInventoryCache().size(), 1);

    // 升级不伤其它数据：user 行与详情缓存保留
    EXPECT_EQ(db->getUser()["id"].toInt(), 9);
    EXPECT_EQ(db->getRecipeDetailCache(5)["name"].toString(), "旧缓存");

    QFile::remove(v0Path);
}

TEST_F(LocalDatabaseTest, 存量v0库升级修复旧结构详情表并清理死表)
{
    // 手工造 v0 文件库："明细列"时代的老结构详情表 + 已拆功能死表（recipes_cache/pending_operations）
    const QString v0Conn = connName + "_v0old";
    const QString v0Path = QDir::tempPath() + QStringLiteral("/gocook_v0old_%1.db").arg(s_counter);
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase("QSQLITE", v0Conn);
        oldDb.setDatabaseName(v0Path);
        ASSERT_TRUE(oldDb.open());
        QSqlQuery q(oldDb);
        ASSERT_TRUE(q.exec("CREATE TABLE user (id INTEGER PRIMARY KEY, username TEXT, token TEXT)"));
        ASSERT_TRUE(q.exec("INSERT INTO user (id, username, token) VALUES (3, 'old', 'tok')"));
        // 老结构：明细列、无 data 列（CREATE IF NOT EXISTS 不会修复它）
        ASSERT_TRUE(q.exec("CREATE TABLE recipe_detail_cache ("
                           "id INTEGER PRIMARY KEY, name TEXT, description TEXT, image_url TEXT)"));
        ASSERT_TRUE(q.exec("INSERT INTO recipe_detail_cache (id, name) VALUES (5, '老详情')"));
        ASSERT_TRUE(q.exec("CREATE TABLE recipes_cache (id INTEGER PRIMARY KEY, data TEXT)"));
        ASSERT_TRUE(q.exec("CREATE TABLE pending_operations (id INTEGER PRIMARY KEY, op TEXT)"));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(v0Conn);

    delete db;
    QSqlDatabase::removeDatabase(connName);
    db = LocalDatabase::createForTesting(v0Path, connName);
    ASSERT_TRUE(db->isOpen());

    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("PRAGMA user_version"));
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 2) << "打开后版本必须推进到 2";

    // 老结构详情表被重建为新结构且可用（老缓存内容按设计丢弃——结构不兼容）
    EXPECT_TRUE(db->saveRecipeDetailCache(6, QVariantMap{{"id", 6}, {"name", "新详情"}}));
    EXPECT_EQ(db->getRecipeDetailCache(6)["name"].toString(), "新详情");
    EXPECT_TRUE(db->getRecipeDetailCache(5).isEmpty()) << "老结构数据不可读（已重建）";

    // 死表清理
    ASSERT_TRUE(q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name IN "
                       "('recipes_cache','pending_operations')"));
    EXPECT_FALSE(q.next()) << "已拆功能死表必须被清理";

    // user 数据保留
    EXPECT_EQ(db->getUser()["id"].toInt(), 3);

    QFile::remove(v0Path);
}

TEST_F(LocalDatabaseTest, 存量v1库升级到v2修复旧结构详情表)
{
    // 手工造 v1 文件库（已跑过首版门控的库）：新结构库存表 + 老结构详情表残留
    const QString v1Conn = connName + "_v1";
    const QString v1Path = QDir::tempPath() + QStringLiteral("/gocook_v1_%1.db").arg(s_counter);
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase("QSQLITE", v1Conn);
        oldDb.setDatabaseName(v1Path);
        ASSERT_TRUE(oldDb.open());
        QSqlQuery q(oldDb);
        ASSERT_TRUE(q.exec("CREATE TABLE user (id INTEGER PRIMARY KEY, username TEXT, token TEXT)"));
        ASSERT_TRUE(q.exec("INSERT INTO user (id, username, token) VALUES (4, 'v1user', 'tok')"));
        ASSERT_TRUE(q.exec("CREATE TABLE inventory_cache ("
                           "user_id INTEGER NOT NULL, ingredient_name TEXT NOT NULL, unit TEXT NOT NULL, "
                           "ordinal INTEGER NOT NULL, data TEXT NOT NULL, updated_at TEXT, "
                           "PRIMARY KEY (user_id, ingredient_name, unit))"));
        ASSERT_TRUE(q.exec("CREATE TABLE inventory_cache_meta (user_id INTEGER PRIMARY KEY, updated_at TEXT NOT NULL)"));
        ASSERT_TRUE(q.exec("CREATE TABLE recipe_detail_cache (id INTEGER PRIMARY KEY, name TEXT)")); // 老结构
        ASSERT_TRUE(q.exec("PRAGMA user_version = 1"));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(v1Conn);

    delete db;
    QSqlDatabase::removeDatabase(connName);
    db = LocalDatabase::createForTesting(v1Path, connName);
    ASSERT_TRUE(db->isOpen());

    QSqlQuery q(QSqlDatabase::database(connName));
    ASSERT_TRUE(q.exec("PRAGMA user_version"));
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.value(0).toInt(), 2) << "已跑过 v1 的库必须经 v2 步自愈";

    // 详情表被修复（可用），user 与库存快照结构保留
    EXPECT_TRUE(db->saveRecipeDetailCache(7, QVariantMap{{"id", 7}, {"name", "修复后"}}));
    EXPECT_EQ(db->getRecipeDetailCache(7)["name"].toString(), "修复后");
    EXPECT_EQ(db->getUser()["id"].toInt(), 4);

    QFile::remove(v1Path);
}

} // namespace
