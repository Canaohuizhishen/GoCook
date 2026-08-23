// test_db_executor.cpp —— executeDb 辅助函数（common/DbExecutor.h）的单元测试
//
// 两部分：
//   ① 单元测试（不依赖数据库）：
//      · 借连接抛 ServiceException(503) → 原样上抛、不翻译（连接池超时契约）
//      · 借连接抛 std::exception → 原样上抛、不翻译（try 外契约）
//      · void / 值返回 lambda 的返回类型推导（if constexpr 分支，编译期）
//   ② 集成测试（依赖真实 PostgreSQL，无 DB 时自动跳过）：
//      · lambda 抛 std::exception → 翻译成 ServiceException(errorMsg)（翻译路径）
//      · lambda 抛 ServiceException → 原样上抛（404/409 不丢）
//      · 成功写操作跨事务可见（提交生效）；异常时写操作回滚
//      · void / 值返回 lambda 的运行时行为
//    连接串：默认本地开发库（与 .env / docker-compose.yml 一致），
//    可用环境变量 GOCOOK_TEST_DB 覆盖；探测失败即跳过（GTEST_SKIP）。

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "common/DbExecutor.h"
#include <gocook/IServices.h>

namespace {

// 借连接即抛业务异常：模拟连接池"满 + 超时"（ConnectionPool.cpp 的 503 语义）
struct TimeoutPool {
    // [[noreturn]]：函数恒抛异常、永不返回——避免"非 void 函数无 return"的 -Wreturn-type 警告
    [[noreturn]] ConnectionPool::ConnectionGuard getConnection(
        std::chrono::milliseconds = std::chrono::milliseconds(5000)) {
        throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
    }
};

// 借连接即抛普通异常：模拟数据库连不上等非业务故障
struct BrokenPool {
    [[noreturn]] ConnectionPool::ConnectionGuard getConnection(
        std::chrono::milliseconds = std::chrono::milliseconds(5000)) {
        throw std::runtime_error("could not connect to database");
    }
};

} // namespace

// ---- 契约测试：借连接阶段的异常发生在 executeDb 的 try 之外，原样上抛 ----

TEST(DbExecutorTest, BorrowTimeoutPropagatesServiceExceptionUnchanged) {
    TimeoutPool pool;
    try {
        executeDb(pool, [](pqxx::work&) {}, "数据库操作失败");
        FAIL() << "expected ServiceException(503) to be thrown";
    } catch (const gocook::services::ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 503);
        EXPECT_EQ(std::string(e.what()), "系统繁忙，请稍后重试");
    }
}

TEST(DbExecutorTest, BorrowStdExceptionPropagatesUntranslated) {
    BrokenPool pool;
    // 契约：借连接异常不翻译成 ServiceException——保持"数据库不可用"的原始故障类型
    EXPECT_THROW(executeDb(pool, [](pqxx::work&) {}, "数据库操作失败"), std::runtime_error);
}

// ---- 编译期：void / 值返回 lambda 的分支推导（锁定 if constexpr 行为）----

static_assert(std::is_void_v<decltype(executeDb(std::declval<TimeoutPool&>(),
                                                [](pqxx::work&) {},
                                                "数据库操作失败"))>,
              "void lambda 应使 executeDb 返回 void");
static_assert(std::is_same_v<decltype(executeDb(std::declval<TimeoutPool&>(),
                                                [](pqxx::work&) -> int { return 1; },
                                                "数据库操作失败")),
                             int>,
              "值返回 lambda 应使 executeDb 返回相同值类型");

// ====================================================================
// 集成测试：需要真实 PostgreSQL（探测失败自动跳过，不阻塞无 DB 环境）
// ====================================================================

namespace {

// 默认连本地开发库（凭据与 .env / docker-compose.yml 一致，非秘密）
const char* kDefaultTestConn =
    "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";

std::string testConnString() {
    const char* env = std::getenv("GOCOOK_TEST_DB");
    return env ? std::string(env) : std::string(kDefaultTestConn);
}

// maxSize=1：池内唯一连接在单线程测试中必然复用——temp 表跨 executeDb 可见，
// 这是"提交/回滚可观测"的前提
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

class DbExecutorDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
    }
};

// 核心翻译路径：lambda 抛 std::exception → ServiceException(errorMsg)，状态码默认 500
TEST_F(DbExecutorDbTest, StdExceptionTranslatedToServiceException) {
    try {
        executeDb(testPool(), [](pqxx::work& txn) {
            txn.exec("SELECT 1");                     // 让事务真正可用
            throw std::runtime_error("boom");         // 模拟解析/文件系统/意外 DB 错误
        }, "自定义业务文案");
        FAIL() << "expected ServiceException";
    } catch (const gocook::services::ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 500);
        EXPECT_EQ(std::string(e.what()), "自定义业务文案");
    }
}

// 业务异常原样上抛：404/409 等状态码不丢
TEST_F(DbExecutorDbTest, ServiceExceptionRethrownUnchanged) {
    try {
        executeDb(testPool(), [](pqxx::work& txn) {
            txn.exec("SELECT 1");
            throw gocook::services::ServiceException("分组名已存在", 409);
        }, "数据库操作失败");
        FAIL() << "expected ServiceException(409)";
    } catch (const gocook::services::ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_EQ(std::string(e.what()), "分组名已存在");
    }
}

// void 分支：写操作提交生效（第二个事务能读到第一个事务的写入）
TEST_F(DbExecutorDbTest, SuccessfulWriteIsCommitted) {
    auto& pool = testPool();
    executeDb(pool, [](pqxx::work& txn) {
        txn.exec("CREATE TEMP TABLE IF NOT EXISTS exec_commit_t (v int)");
        txn.exec("DELETE FROM exec_commit_t");
        txn.exec("INSERT INTO exec_commit_t VALUES (7)");
    }, "数据库操作失败");
    int cnt = executeDb(pool, [](pqxx::work& txn) {
        return txn.exec("SELECT COUNT(*) FROM exec_commit_t")[0][0].as<int>();
    }, "数据库操作失败");
    EXPECT_EQ(cnt, 1);
}

// 值返回分支：映射结果正确返回
TEST_F(DbExecutorDbTest, ValueLambdaReturnsResult) {
    int v = executeDb(testPool(), [](pqxx::work& txn) {
        return txn.exec("SELECT 42")[0][0].as<int>();
    }, "数据库操作失败");
    EXPECT_EQ(v, 42);
}

// 异常时回滚：写操作 + 抛异常 → 数据不落库
TEST_F(DbExecutorDbTest, ExceptionRollsBackWrite) {
    auto& pool = testPool();
    executeDb(pool, [](pqxx::work& txn) {
        txn.exec("CREATE TEMP TABLE IF NOT EXISTS exec_rollback_t (v int)");
        txn.exec("DELETE FROM exec_rollback_t");
    }, "数据库操作失败");
    EXPECT_THROW(executeDb(pool, [](pqxx::work& txn) {
        txn.exec("INSERT INTO exec_rollback_t VALUES (1)");
        throw std::runtime_error("boom");
    }, "数据库操作失败"), gocook::services::ServiceException);
    int cnt = executeDb(pool, [](pqxx::work& txn) {
        return txn.exec("SELECT COUNT(*) FROM exec_rollback_t")[0][0].as<int>();
    }, "数据库操作失败");
    EXPECT_EQ(cnt, 0);
}
