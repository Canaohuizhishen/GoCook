// test_auth_vm.cpp —— AuthViewModel 自动登录（checkAutoLogin）的会话判定契约
//
// 覆盖自动登录校验的三态判定（断网/服务器繁忙启动不得误删凭证与库存快照）：
//   A1 瞬时失败（网络层错误，连接拒绝）→ 保留凭证 + 快照 + token，乐观登录进入主界面
//   A2 瞬时失败（503 服务器繁忙）      → 同上
//   A3 服务端明确拒绝（401 无效令牌）  → 清理凭证与快照，保持登出
//   A4 成功路径                        → loggedIn + 资料字段填充
//
// 判定机制：校验窗口内的 401 由 HttpGoCookApi 的 unauthorizedHandler 标记——
// 不经字符串匹配、不改任何用户可见文案；本文件端到端锁定该机制（含回调顺序不变量：
// sendRaw 先触发 unauthorizedHandler 再回调业务结果）。

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QString>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include "AuthViewModel.h"
#include "HttpGoCookApi.h"
#include "LocalDatabase.h"
#include <httplib/httplib.h>

namespace {

// 轮询事件循环直到条件满足或超时（QNetworkAccessManager 是异步的，需要事件循环推进）
template <typename Fn>
bool waitUntil(const Fn& fn, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!fn()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (timer.elapsed() > timeoutMs)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return true;
}

// 自动登录校验桩：GET /api/users/me 按 mode 返回（0=200 成功 / 1=401 无效令牌 / 2=503 繁忙）
class UsersMeStubServer {
public:
    std::atomic<int> mode{0};
    std::atomic<int> reqCount{0};

    UsersMeStubServer()
    {
        svr.Get("/api/users/me", [this](const httplib::Request&, httplib::Response& res) {
            reqCount++;
            switch (mode.load()) {
            case 1:
                res.status = 401;
                res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
                break;
            case 2:
                res.status = 503;
                res.set_content(R"({"error":"系统繁忙，请稍后重试"})", "application/json");
                break;
            default:
                res.status = 200;
                res.set_content(R"({"id":1,"username":"tester","display_name":"测试","email":"t@test.com",)"
                                R"("phone":"13800000000","avatar_url":"/uploads/a.png","created_at":"2026-01-01"})",
                                "application/json");
                break;
            }
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("UsersMeStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running()
        // 显式暴露启动失败；同时消除"端口已绑定但尚未开始监听"的竞态窗口
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("UsersMeStubServer: failed to start");
    }

    ~UsersMeStubServer()
    {
        svr.stop();
        if (th.joinable())
            th.join();
    }

    std::string baseUrl() const { return "http://127.0.0.1:" + std::to_string(port); }

private:
    httplib::Server svr;
    int port = 0;
    std::thread th;
};

class AuthVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 独立内存库注入（不触生产数据路径）；预置上次会话：user 行 + 库存快照
        connName = QStringLiteral("gocook_authtest_%1").arg(s_counter++);
        testDb = LocalDatabase::createForTesting(QStringLiteral(":memory:"), connName);
        ASSERT_TRUE(testDb != nullptr);
        ASSERT_TRUE(testDb->isOpen());
        ASSERT_TRUE(testDb->saveUser(1, QStringLiteral("tester"), QStringLiteral("token-A")));
        QVariantList snapshot;
        snapshot << QVariantMap{{"id", 7}, {"ingredientName", "番茄"}, {"quantity", 3.0}, {"unit", "个"}};
        ASSERT_TRUE(testDb->saveInventoryCache(snapshot));
    }

    void TearDown() override
    {
        delete testDb;
        testDb = nullptr;
        QSqlDatabase::removeDatabase(connName);
    }

    HttpGoCookApi api;
    LocalDatabase *testDb = nullptr;
    QString connName;
    static int s_counter;
};

int AuthVmTest::s_counter = 0;

// ==================== A1：瞬时失败（网络层错误）→ 保留凭证+快照，乐观登录 ====================
TEST_F(AuthVmTest, 断网启动保留凭证快照并乐观登录)
{
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 无监听：连接拒绝 → 网络层错误

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";

    EXPECT_TRUE(vm.loggedIn()) << "瞬时故障应乐观登录（断网启动可进主界面读快照）";
    EXPECT_EQ(vm.userId(), 1);
    EXPECT_EQ(vm.username(), QStringLiteral("tester"));
    EXPECT_FALSE(api.authToken().empty()) << "token 保留（联网后可自证有效性）";
    EXPECT_FALSE(testDb->getUser().isEmpty()) << "凭证不得因网络故障被清理";
    EXPECT_TRUE(testDb->hasInventoryCache()) << "快照必须保留（离线兜底的数据基础）";
}

// ==================== A2：瞬时失败（503）→ 同上 ====================
TEST_F(AuthVmTest, 服务器繁忙启动保留凭证快照并乐观登录)
{
    UsersMeStubServer stub;
    stub.mode = 2; // 503 服务器繁忙
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";

    EXPECT_TRUE(vm.loggedIn()) << "503 属瞬时故障，不得以登出+清数据回应";
    EXPECT_FALSE(api.authToken().empty());
    EXPECT_FALSE(testDb->getUser().isEmpty());
    EXPECT_TRUE(testDb->hasInventoryCache());
}

// ==================== A3：服务端明确拒绝（401）→ 清理凭证与快照 ====================
TEST_F(AuthVmTest, 令牌被拒启动清理凭证与快照)
{
    UsersMeStubServer stub;
    stub.mode = 1; // 401 无效令牌
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";

    EXPECT_FALSE(vm.loggedIn()) << "401 = 会话确已失效，不得乐观登录";
    EXPECT_TRUE(api.authToken().empty()) << "token 必须清理";
    EXPECT_TRUE(testDb->getUser().isEmpty()) << "401 必须清理凭证";
    EXPECT_FALSE(testDb->hasInventoryCache()) << "401 必须连带清理快照（防陈旧串台）";
    EXPECT_EQ(stub.reqCount.load(), 1) << "校验请求确实发出过一次";
}

// ==================== A4：成功路径基线 ====================
TEST_F(AuthVmTest, 校验成功进入登录态并填充资料)
{
    UsersMeStubServer stub;
    stub.mode = 0; // 200
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";

    EXPECT_TRUE(vm.loggedIn());
    EXPECT_EQ(vm.userId(), 1);
    EXPECT_EQ(vm.username(), QStringLiteral("tester"));
    EXPECT_EQ(vm.profileEmail(), QStringLiteral("t@test.com"));
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/a.png"));
    EXPECT_FALSE(testDb->getUser().isEmpty());
    EXPECT_TRUE(testDb->hasInventoryCache()) << "成功路径不得误删快照";
}

} // namespace
