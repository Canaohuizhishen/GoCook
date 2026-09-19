// test_notification_watermark_integration.cpp —— 通知已读水位模型的 PgUserRepository 集成测试（依赖真实 PostgreSQL）
//
// 覆盖 v2.23 水位模型的 SQL 语义（此前只有 mock 委派测试，测不到 SQL 行为）：
//   · getUnreadSummary：按 type 分组统计 id > 水位的未读数；无水位行 = 全部未读
//   · updateReadWatermark：GREATEST 合并（回退上报不改写）、上报值收敛到频道当前最大 id
//   · getNotifications：列表 is_read 由"id > 水位"动态计算；各分类水位互不影响
//   · 公告频道：has_new_announcement 由公告 id > 公告水位判定，与通知频道相互独立
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试"前缀 + 专用用户，
// SetUp/TearDown 清理，不影响业务数据。（模式同 test_favorites_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>

#include "../common/ConnectionPool.h"
#include "../repositories/PgUserRepository.h"
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

class NotificationWatermarkDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        cleanup();
        userId_ = ensureUser();
    }

    void TearDown() override { cleanup(); }

    // 注意：连接池 maxSize=1，helper 内必须用完即还（作用域块），再调 repo 方法

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        // notifications / read_watermarks 均随 users 级联删除；公告为全局数据不清理
        ntxn.exec("DELETE FROM users WHERE username = '集成测试水位用户'");
    }

    /// 创建（或复用）测试用户，返回用户 id
    static int ensureUser() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ('集成测试水位用户', 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id");
        return r[0][0].as<int>();
    }

    /// 插入一条通知（type: review/interaction），返回通知 id
    static int insertNotification(int userId, const std::string& type) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO notifications (user_id, title, content, type) "
            "VALUES ($1, '集成测试通知', '内容', $2) RETURNING id",
            pqxx::params{userId, type});
        return r[0][0].as<int>();
    }

    /// 当前公告表最大 id（0 = 无公告）
    static int maxAnnouncementId() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        return ntxn.exec("SELECT COALESCE(MAX(id), 0) FROM announcements")[0][0].as<int>();
    }

    int userId_ = 0;
};

// ==================== 水位汇总 / 动态已读 / 分类互不影响 ====================
TEST_F(NotificationWatermarkDbTest, 水位汇总与动态已读语义)
{
    PgUserRepository repo(testPool());

    const int r1 = insertNotification(userId_, "review");
    const int r2 = insertNotification(userId_, "review");
    const int i1 = insertNotification(userId_, "interaction");
    (void)i1;

    // 无水位行 = 全部未读
    auto s0 = repo.getUnreadSummary(userId_);
    EXPECT_EQ(s0.unread_review, 2);
    EXPECT_EQ(s0.unread_interaction, 1);

    // 推进 review 水位到 r1：r2 仍未读；interaction 不受影响
    repo.updateReadWatermark(userId_, "review", r1);
    auto s1 = repo.getUnreadSummary(userId_);
    EXPECT_EQ(s1.unread_review, 1);
    EXPECT_EQ(s1.unread_interaction, 1) << "其他频道水位互不影响";

    // 列表 is_read 动态计算：review 列表 r1 已读、r2 未读
    auto list = repo.getNotifications(userId_, 1, 20, "review");
    ASSERT_EQ(list.data.size(), 2u);
    for (const auto& item : list.data) {
        if (item.id == r1)
            EXPECT_TRUE(item.is_read) << "id <= 水位 = 已读";
        if (item.id == r2)
            EXPECT_FALSE(item.is_read) << "id > 水位 = 未读";
    }

    // interaction 列表水位未推进，仍为未读
    auto ilist = repo.getNotifications(userId_, 1, 20, "interaction");
    ASSERT_EQ(ilist.data.size(), 1u);
    EXPECT_FALSE(ilist.data[0].is_read);

    // 推进到 r2：review 清零
    repo.updateReadWatermark(userId_, "review", r2);
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 0);
}

// ==================== 水位只前进不回退，且收敛到当前最大 id ====================
TEST_F(NotificationWatermarkDbTest, 水位只前进不回退且收敛到当前最大id)
{
    PgUserRepository repo(testPool());
    const int r1 = insertNotification(userId_, "review");
    const int r2 = insertNotification(userId_, "review");

    repo.updateReadWatermark(userId_, "review", r2);
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 0);

    // 回退上报：GREATEST 合并，水位保持 r2
    repo.updateReadWatermark(userId_, "review", r1);
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 0) << "水位不得回退";

    // 越界上报：收敛到当前最大 id（不给未来通知"预盖章"）
    repo.updateReadWatermark(userId_, "review", r2 + 100000);
    const int r3 = insertNotification(userId_, "review");
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 1) << "越界上报被收敛，未来通知不被吞";

    // 再推进到 r3 清除
    repo.updateReadWatermark(userId_, "review", r3);
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 0);
}

// ==================== 公告频道独立水位（红点） ====================
TEST_F(NotificationWatermarkDbTest, 公告频道独立水位)
{
    PgUserRepository repo(testPool());

    const int maxAnnId = maxAnnouncementId();
    if (maxAnnId == 0)
        GTEST_SKIP() << "公告表为空（先执行种子数据）";

    // 该用户从未看过公告：有红点；且不产生任何通知类未读
    const auto before = repo.getUnreadSummary(userId_);
    EXPECT_TRUE(before.has_new_announcement);
    EXPECT_EQ(before.unread_review, 0) << "公告水位不影响通知频道";
    EXPECT_EQ(before.unread_interaction, 0);

    // 看过（上报最大公告 id）：红点消失
    repo.updateReadWatermark(userId_, "announcement", maxAnnId);
    EXPECT_FALSE(repo.getUnreadSummary(userId_).has_new_announcement);

    // 通知频道仍未读的事实不被公告水位影响
    insertNotification(userId_, "review");
    EXPECT_EQ(repo.getUnreadSummary(userId_).unread_review, 1);
}
