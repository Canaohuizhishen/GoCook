// test_user_token_integration.cpp —— 邮箱验证码/令牌有效期的 PgUserRepository 集成测试（依赖真实 PostgreSQL）
//
// 锁定「令牌过期时间常量单点化」后的 SQL 语义：注册验证码（pending_registrations）与
// 密码重置令牌（password_reset_tokens）两条链路的 expires_at 都来自同一常量
// EMAIL_CODE_EXPIRY_MINUTES。期望值在测试中硬编码为 15 分钟——常量被改动而 SQL
// 未跟随、或 make_interval 参数绑定失效时，此测试必须失败。
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试"前缀 + 专用用户，
// SetUp/TearDown 清理，不影响业务数据。（模式同 test_notification_watermark_integration.cpp）

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

// 两条链路 TTL 期望的硬编码语义值（秒）：= EMAIL_CODE_EXPIRY_MINUTES 当前取值 15 分钟
constexpr double kExpectedTtlSeconds = 15 * 60;

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

class UserTokenExpiryDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        cleanup();
        userId_ = ensureUser();
    }

    void TearDown() override { cleanup(); }

    // 注意：连接池 maxSize=1，helper 内必须用完即还（作用域块），再调 repo 方法

    static constexpr const char* kUsername = "集成测试令牌用户";
    static constexpr const char* kEmail = "集成测试令牌@example.com";

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        // password_reset_tokens 随 users 级联删除；pending_registrations 按邮箱清理
        ntxn.exec("DELETE FROM users WHERE username = $1",
                  pqxx::params{std::string(kUsername)});
        ntxn.exec("DELETE FROM pending_registrations WHERE email = $1",
                  pqxx::params{std::string(kEmail)});
    }

    /// 创建（或复用）测试用户，返回用户 id
    static int ensureUser() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id",
            pqxx::params{std::string(kUsername)});
        return r[0][0].as<int>();
    }

    /// 密码重置令牌距过期的剩余秒数
    static double resetTokenRemainingSeconds(const std::string& token) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "SELECT EXTRACT(EPOCH FROM (expires_at - NOW())) "
            "FROM password_reset_tokens WHERE token = $1",
            pqxx::params{token});
        return r[0][0].as<double>();
    }

    /// 注册验证码距过期的剩余秒数
    static double pendingCodeRemainingSeconds(const std::string& email) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "SELECT EXTRACT(EPOCH FROM (expires_at - NOW())) "
            "FROM pending_registrations WHERE email = $1",
            pqxx::params{email});
        return r[0][0].as<double>();
    }

    int userId_ = 0;
};

// ==================== 密码重置令牌：expires_at = NOW() + 常量（15 分钟） ====================
TEST_F(UserTokenExpiryDbTest, 重置令牌过期时间与常量一致)
{
    PgUserRepository repo(testPool());

    const std::string token = "集成测试重置令牌-1";
    repo.createPasswordResetToken(userId_, token);

    const double remain = resetTokenRemainingSeconds(token);
    EXPECT_GT(remain, kExpectedTtlSeconds - 10.0) << "TTL 明显短于 15 分钟（SQL 未跟随常量？）";
    EXPECT_LE(remain, kExpectedTtlSeconds) << "TTL 不得超出 15 分钟";
}

// ==================== 注册验证码：expires_at = NOW() + 常量（15 分钟） ====================
TEST_F(UserTokenExpiryDbTest, 注册验证码过期时间与常量一致)
{
    PgUserRepository repo(testPool());

    repo.upsertPendingRegistration(kUsername, "x", kEmail, "123456");

    const double remain = pendingCodeRemainingSeconds(kEmail);
    EXPECT_GT(remain, kExpectedTtlSeconds - 10.0) << "TTL 明显短于 15 分钟（SQL 未跟随常量？）";
    EXPECT_LE(remain, kExpectedTtlSeconds) << "TTL 不得超出 15 分钟";
}
