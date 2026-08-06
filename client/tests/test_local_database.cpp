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

// ==================== fixture ====================

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

TEST_F(LocalDatabaseTest, 详情缓存老表结构迁移重建) {
    // 手工建老结构（明细列）的库文件；作用域结束后再 removeDatabase，避免 Qt 告警
    const QString oldConn = connName + "_cache";
    const QString oldPath = QDir::tempPath() + QStringLiteral("/gocook_cache_migration_%1.db").arg(s_counter);
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase("QSQLITE", oldConn);
        oldDb.setDatabaseName(oldPath);
        ASSERT_TRUE(oldDb.open());
        QSqlQuery q(oldDb);
        ASSERT_TRUE(q.exec("CREATE TABLE recipe_detail_cache ("
                           "id INTEGER PRIMARY KEY, name TEXT, image_url TEXT, "
                           "prep_time_minutes INTEGER, avg_rating REAL)"));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(oldConn);

    // 重开 → createTables 检测无 data 列 → 重建为新结构，新 API 可直接使用
    delete db;
    QSqlDatabase::removeDatabase(connName); // 先注销 :memory: 连接，避免重复连接名告警
    db = LocalDatabase::createForTesting(oldPath, connName);
    ASSERT_TRUE(db->isOpen());
    EXPECT_TRUE(db->saveRecipeDetailCache(3, QVariantMap{{"id", 3}, {"name", "C"}}));
    EXPECT_EQ(db->getRecipeDetailCache(3)["name"].toString(), "C");

    QFile::remove(oldPath);
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

// ==================== 离线队列表清理（已拆除的功能） ====================

TEST_F(LocalDatabaseTest, 老库遗留的离线队列表被清理) {
    // 用文件库手工建一个遗留的 pending_operations 表（旧版本功能），
    // 而非 :memory:（新空库本来就没有该表，断言恒过、测不到清理逻辑）
    const QString legacyConn = connName + "_pending";
    const QString legacyPath = QDir::tempPath() + QStringLiteral("/gocook_pending_%1.db").arg(s_counter);
    {
        QSqlDatabase oldDb = QSqlDatabase::addDatabase("QSQLITE", legacyConn);
        oldDb.setDatabaseName(legacyPath);
        ASSERT_TRUE(oldDb.open());
        QSqlQuery q(oldDb);
        ASSERT_TRUE(q.exec("CREATE TABLE pending_operations ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "operation TEXT, data TEXT, user_id INTEGER DEFAULT 0)"));
        ASSERT_TRUE(q.exec("INSERT INTO pending_operations (operation, data) "
                           "VALUES ('http_request', '{}')"));
        oldDb.close();
    }
    QSqlDatabase::removeDatabase(legacyConn);

    // 重开同一文件库 → createTables 应删除遗留表（功能已拆除）
    delete db;
    QSqlDatabase::removeDatabase(connName); // 先注销 :memory: 连接，避免重复连接名告警
    db = LocalDatabase::createForTesting(legacyPath, connName);
    ASSERT_TRUE(db->isOpen());

    QSqlQuery check(QSqlDatabase::database(connName));
    ASSERT_TRUE(check.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='pending_operations'"));
    EXPECT_FALSE(check.next()) << "pending_operations 表应已被清理";

    QFile::remove(legacyPath);
}

} // namespace
