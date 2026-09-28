// test_token_revocation_integration.cpp —— JWT 主动吊销（token_version）集成测试（依赖真实 PostgreSQL）
//
// 锁定语义（T-1）：
//   · 令牌携带的 ver 必须与 users.token_version 一致才有效（未携带 ver 按 0——部署前
//     签发的旧格式令牌，改密前仍然有效）；
//   · 改密（changePassword）/ 重置密码（resetPasswordAndMarkTokenUsed）在写密码的同一
//     语句里自增版本 → 所有旧令牌立即被 AuthMiddleware 拒绝；
//   · 用户不存在（含注销）→ 一律拒绝（fail-closed）。
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。测试数据使用"集成测试"前缀 + 专用用户，
// SetUp/TearDown 清理，不影响业务数据。（模式同 test_user_token_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#include <jwt-cpp/jwt.h>

#include "../common/ConnectionPool.h"
#include "../middleware/auth_middleware.h"
#include "../repositories/PgUserRepository.h"

namespace {

const char* kDefaultTestConn =
    "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";

constexpr const char* kTestSecret = "token-revocation-integration-test-secret";

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

/// 自签 JWT：iss=GoCook／userId／username／role／exp，可选 ver（nullopt = 不携带，兼容路径）
std::string mintToken(int userId, std::optional<int> ver) {
    const auto now = std::chrono::system_clock::now();
    auto builder = jwt::create()
                       .set_issuer("GoCook")
                       .set_type("JWS")
                       .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
                       .set_payload_claim("username", jwt::claim(std::string("tester")))
                       .set_payload_claim("role", jwt::claim(std::string("user")))
                       .set_issued_at(now)
                       .set_expires_at(now + std::chrono::hours(1));
    if (ver.has_value()) {
        builder.set_payload_claim("ver", jwt::claim(std::to_string(*ver)));
    }
    return builder.sign(jwt::algorithm::hs256{kTestSecret});
}

constexpr const char* kUsername = "集成测试吊销用户";
constexpr const char* kEmail = "集成测试吊销@example.com";

} // namespace

class TokenRevocationDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        cleanup();
        userId_ = ensureUser();
        repo_ = std::make_unique<PgUserRepository>(testPool());
        auth_ = std::make_unique<AuthMiddleware>(kTestSecret, *repo_);
    }

    void TearDown() override { cleanup(); }

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("DELETE FROM users WHERE username = $1",
                  pqxx::params{std::string(kUsername)});
    }

    /// 创建测试用户（密码哈希为占位——本文件不经 UserService 登录，只验证令牌吊销链路）
    static int ensureUser() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        auto row = ntxn.exec(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, 'x', $2) RETURNING id",
            pqxx::params{std::string(kUsername), std::string(kEmail)});
        return row[0]["id"].as<int>();
    }

    bool tokenValid(const std::string& token) const {
        return auth_->authenticate("Bearer " + token).valid;
    }

    int userId_ = 0;
    std::unique_ptr<PgUserRepository> repo_;
    std::unique_ptr<AuthMiddleware> auth_;
};

// ==================== 基线：版本一致才通过 ====================

TEST_F(TokenRevocationDbTest, 初始版本为零且携带ver0的令牌通过) {
    ASSERT_EQ(repo_->getTokenVersion(userId_).value_or(-1), 0);
    EXPECT_TRUE(tokenValid(mintToken(userId_, 0)));
}

TEST_F(TokenRevocationDbTest, ver缺失按零兼容且改密前有效) {
    EXPECT_TRUE(tokenValid(mintToken(userId_, std::nullopt)));
}

TEST_F(TokenRevocationDbTest, 版本不匹配的令牌被拒) {
    // 当前版本 0，携带 ver=1 的令牌必须被拒
    EXPECT_FALSE(tokenValid(mintToken(userId_, 1)));
}

// ==================== 改密：旧令牌立即失效 ====================

TEST_F(TokenRevocationDbTest, 改密后旧令牌立即失效新版本令牌通过) {
    const std::string oldToken = mintToken(userId_, 0);
    const std::string legacyToken = mintToken(userId_, std::nullopt);
    EXPECT_TRUE(tokenValid(oldToken));

    repo_->changePassword(userId_, "new-hash");

    ASSERT_EQ(repo_->getTokenVersion(userId_).value_or(-1), 1);
    EXPECT_FALSE(tokenValid(oldToken));     // ver=0 旧令牌被吊销
    EXPECT_FALSE(tokenValid(legacyToken));  // 缺 ver 兼容路径同样被吊销
    EXPECT_TRUE(tokenValid(mintToken(userId_, 1)));  // 新签发令牌有效
}

// ==================== 重置密码：同样吊销 ====================

TEST_F(TokenRevocationDbTest, 重置密码后旧令牌立即失效) {
    const std::string oldToken = mintToken(userId_, 0);
    EXPECT_TRUE(tokenValid(oldToken));

    repo_->createPasswordResetToken(userId_, "integration-reset-token");
    repo_->resetPasswordAndMarkTokenUsed(userId_, "new-hash", "integration-reset-token");

    ASSERT_EQ(repo_->getTokenVersion(userId_).value_or(-1), 1);
    EXPECT_FALSE(tokenValid(oldToken));
    EXPECT_TRUE(tokenValid(mintToken(userId_, 1)));
}

// ==================== 注销：一律拒绝 ====================

TEST_F(TokenRevocationDbTest, 注销后令牌一律拒绝) {
    const std::string token = mintToken(userId_, 0);
    EXPECT_TRUE(tokenValid(token));

    repo_->deleteAccount(userId_);

    EXPECT_FALSE(repo_->getTokenVersion(userId_).has_value());
    EXPECT_FALSE(tokenValid(token));
    EXPECT_FALSE(tokenValid(mintToken(userId_, 0)));
}
