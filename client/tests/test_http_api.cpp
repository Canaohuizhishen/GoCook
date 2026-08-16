// HttpGoCookApi 网络层错误处理测试
//
// 覆盖（对应 NetDemo README 第五节① 抓出的真 bug 及后续统一修复）：
//   T1  业务 400（空关键词）不触发 GET 重试 —— 回归测试：修复前会连发 maxRetries+1 次
//   T2  401 → unauthorized 信号 + 服务端精确文案
//   T3  updateFavoriteItem（PATCH，绕过 sendRequest）401 也触发 unauthorized（本轮补齐的行为）
//   T4  uploadAvatar（绕过 sendRequest）401 文案统一
//   T5  断网（连接拒绝，statusCode=0）→ 统一网络文案，不再抛空 body
//   T6  真正的网络错误仍然重试（原请求 + maxRetries 次，TCP 层计数）
//   T7  E2E（可选）：真实 GoCook 服务端（设置 GOCOOK_E2E_BASE 时启用）
//
// 桩服务器用 httplib（与 GoCook 服务端同款），测试不依赖真实服务端。

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QString>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "HttpGoCookApi.h"
#include <httplib/httplib.h>

#ifdef Q_OS_UNIX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

// 轮询事件循环直到 done 或超时（QNetworkAccessManager 是异步的，需要事件循环推进）
bool waitUntil(const std::atomic<bool>& done, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!done.load()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (timer.elapsed() > timeoutMs)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // 让最后一个回调之后的挂起事件也跑完（如 deleteLater）
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return true;
}

// 模拟 GoCook 服务端错误响应的测试桩
class StubServer {
public:
    std::atomic<int> searchReqCount{0};
    std::atomic<int> avatarReqCount{0};

    StubServer()
    {
        // 与真实服务端 RecipeHandler::searchRecipes 完全一致的 400 响应
        svr.Get("/api/recipes/search", [this](const httplib::Request&, httplib::Response& res) {
            searchReqCount++;
            res.status = 400;
            res.set_content(R"({"error":"keyword cannot be empty"})", "application/json");
        });
        // 与真实服务端一致的 401 响应
        svr.Get("/api/me", [](const httplib::Request&, httplib::Response& res) {
            res.status = 401;
            res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
        });
        svr.Patch("/api/users/me/favorites/1", [](const httplib::Request&, httplib::Response& res) {
            res.status = 401;
            res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
        });
        svr.Post("/api/users/me/avatar", [this](const httplib::Request&, httplib::Response& res) {
            avatarReqCount++;
            res.status = 401;
            res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
        });
        svr.Post("/api/recipes/1/image", [](const httplib::Request&, httplib::Response& res) {
            res.status = 401;
            res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
        });
        svr.Post("/api/recipes/1/steps/0/image", [](const httplib::Request&, httplib::Response& res) {
            res.status = 401;
            res.set_content(R"({"error":"无效的访问令牌"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("StubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });
    }

    ~StubServer()
    {
        svr.stop();
        if (th.joinable())
            th.join();
    }

    std::string baseUrl() const
    {
        return "http://127.0.0.1:" + std::to_string(port);
    }

private:
    httplib::Server svr;
    int port = -1;
    std::thread th;
};

// TCP 层"连接即断开"的服务：accept 后稍等让客户端把请求发完，再关闭连接。
// 客户端表现为"收到连接但无 HTTP 响应"（statusCode=0 → 网络错误），
// 用 accept 次数精确统计客户端实际发起了几次连接（验证重试次数）。
class TcpRefuseSink {
public:
    std::atomic<int> accepted{0};

    TcpRefuseSink()
    {
#ifdef Q_OS_UNIX
        listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0)
            throw std::runtime_error("TcpRefuseSink: socket failed");
        int one = 1;
        ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            throw std::runtime_error("TcpRefuseSink: bind failed");
        socklen_t len = sizeof(addr);
        if (::getsockname(listenFd, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
            throw std::runtime_error("TcpRefuseSink: getsockname failed");
        port = ntohs(addr.sin_port);
        if (::listen(listenFd, 16) != 0)
            throw std::runtime_error("TcpRefuseSink: listen failed");
        th = std::thread([this]() { acceptLoop(); });
#else
        GTEST_SKIP() << "TcpRefuseSink 仅支持 Unix";
#endif
    }

    ~TcpRefuseSink()
    {
#ifdef Q_OS_UNIX
        stop = true;
        ::shutdown(listenFd, SHUT_RDWR);
        ::close(listenFd);
        if (th.joinable())
            th.join();
#endif
    }

    std::string baseUrl() const
    {
        return "http://127.0.0.1:" + std::to_string(port);
    }

private:
#ifdef Q_OS_UNIX
    void acceptLoop()
    {
        while (!stop.load()) {
            int c = ::accept(listenFd, nullptr, nullptr);
            if (c < 0)
                break;
            accepted++;
            // 读取并丢弃请求数据，直到收到完整请求头——确保客户端认为请求已送达，
            // 再关闭连接 → RemoteHostClosedError(statusCode=0)，避免 QNAM 内部自动重发干扰计数
            char buf[4096];
            ssize_t n;
            std::string received;
            while ((n = ::recv(c, buf, sizeof(buf), 0)) > 0) {
                received.append(buf, static_cast<size_t>(n));
                if (received.find("\r\n\r\n") != std::string::npos)
                    break;
            }
            ::close(c);
        }
    }
    int listenFd = -1;
#endif
    int port = -1;
    std::atomic<bool> stop{false};
    std::thread th;
};

class HttpApiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setRetryDelay(50); // 测试里缩短重试间隔
    }

    HttpGoCookApi api;
};

} // namespace

// ==================== T1：业务 400 不触发 GET 重试（NetDemo bug 回归） ====================
TEST_F(HttpApiTest, Business400_NotRetried)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setMaxRetries(2); // 即使开启重试，业务 400 也不得重发

    std::atomic<bool> done{false};
    bool ok = true;
    QString err;
    int cbCount = 0;
    api.get("/api/recipes/search?keyword=&page=1&size=5",
            [&](bool s, const QString& e, const QJsonDocument&) {
                ok = s;
                err = e;
                cbCount++;
                done = true;
            });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err.toStdString(), "keyword cannot be empty");
    EXPECT_EQ(cbCount, 1) << "回调只能触发一次";
    EXPECT_EQ(stub.searchReqCount.load(), 1) << "业务 400 不得被重试：服务端只应收到 1 次请求";
}

// ==================== T2：401 → unauthorized 信号 + 服务端精确文案 ====================
TEST_F(HttpApiTest, Unauthorized401_SignalAndMessage)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> unauthorizedCalled{false};
    QObject::connect(&api, &HttpGoCookApi::unauthorized, [&]() { unauthorizedCalled = true; });

    std::atomic<bool> done{false};
    bool ok = true;
    QString err;
    api.get("/api/me", [&](bool s, const QString& e, const QJsonDocument&) {
        ok = s;
        err = e;
        done = true;
    });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_TRUE(unauthorizedCalled.load()) << "401 必须触发 unauthorized 信号";
    EXPECT_EQ(err.toStdString(), "无效的访问令牌") << "应使用服务端精确文案";
}

// ==================== T3：updateFavoriteItem（PATCH）401 也触发 unauthorized ====================
TEST_F(HttpApiTest, UpdateFavoriteItem401_TriggersUnauthorized)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> unauthorizedCalled{false};
    QObject::connect(&api, &HttpGoCookApi::unauthorized, [&]() { unauthorizedCalled = true; });

    std::atomic<bool> done{false};
    bool ok = true;
    std::string err;
    gocook::models::UpdateFavoriteRequest req;
    api.updateFavoriteItem(1, req, [&](bool s, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_TRUE(unauthorizedCalled.load()) << "绕过 sendRequest 的 PATCH 也必须触发登出";
    EXPECT_EQ(err, "无效的访问令牌");
}

// ==================== T4：uploadAvatar（绕过 sendRequest）401 文案统一 ====================
TEST_F(HttpApiTest, UploadAvatar401_UnifiedMessage)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> unauthorizedCalled{false};
    QObject::connect(&api, &HttpGoCookApi::unauthorized, [&]() { unauthorizedCalled = true; });

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile img(dir.filePath("avatar.png"));
    ASSERT_TRUE(img.open(QIODevice::WriteOnly));
    img.write("fake-png-bytes");
    img.close();

    std::atomic<bool> done{false};
    bool ok = true;
    std::string err;
    api.uploadAvatar(img.fileName().toStdString(),
                     [&](bool s, const gocook::models::AvatarUploadResponse&, const std::string& e) {
                         ok = s;
                         err = e;
                         done = true;
                     });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_TRUE(unauthorizedCalled.load());
    EXPECT_EQ(err, "无效的访问令牌") << "401 应返回服务端精确文案而不是硬编码\"未授权\"";
    EXPECT_EQ(stub.avatarReqCount.load(), 1);
}

// ==================== T5：断网（连接拒绝）→ 统一网络文案 ====================
TEST_F(HttpApiTest, NetworkError_ConnectionRefused_UnifiedMessage)
{
    // 127.0.0.1:1 无任何监听 → 立即连接拒绝，statusCode=0
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));

    std::atomic<bool> done{false};
    bool ok = true;
    QString err;
    api.get("/api/me", [&](bool s, const QString& e, const QJsonDocument&) {
        ok = s;
        err = e;
        done = true;
    });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err.toStdString(), "网络连接失败，请检查网络") << "断网必须给出网络文案，而不是空 body";
}

// ==================== T6：真正的网络错误仍然重试（连接尝试多于不重试基线） ====================
// 注：QNetworkAccessManager 对幂等请求在"响应前连接被关闭"时会内部隐藏重发
// （实测基线 1 次外部请求可产生 3 次 TCP 连接），精确计数会被污染，
// 因此用"开启重试后连接尝试必须多于不重试基线"来验证 HttpGoCookApi 的重试确实在发新请求。
TEST_F(HttpApiTest, NetworkError_StillRetried_TcpLevelCount)
{
    TcpRefuseSink sinkBase;
    {
        HttpGoCookApi baseApi;
        baseApi.setBaseUrl(QString::fromStdString(sinkBase.baseUrl()));
        baseApi.setMaxRetries(0);
        std::atomic<bool> done{false};
        baseApi.get("/api/me", [&](bool, const QString&, const QJsonDocument&) { done = true; });
        ASSERT_TRUE(waitUntil(done, 10000)) << "基线请求超时";
    }
    const int baseline = sinkBase.accepted.load();

    TcpRefuseSink sink;
    api.setBaseUrl(QString::fromStdString(sink.baseUrl()));
    api.setMaxRetries(3);

    std::atomic<bool> done{false};
    bool ok = true;
    QString err;
    api.get("/api/me", [&](bool s, const QString& e, const QJsonDocument&) {
        ok = s;
        err = e;
        done = true;
    });

    ASSERT_TRUE(waitUntil(done, 15000)) << "回调超时";
    const int retried = sink.accepted.load();
    EXPECT_FALSE(ok);
    EXPECT_EQ(err.toStdString(), "网络连接失败，请检查网络");
    EXPECT_GT(retried, baseline) << "开启重试后必须发起比基线更多的连接尝试";
}

// ==================== T7：E2E —— 真实 GoCook 服务端（可选） ====================
// 用法：GOCOOK_E2E_BASE=http://127.0.0.1:8080 ./test_client
// 未设置时跳过。账号 testuser/test123（server/sql/seed_test_data.sql）。
TEST(HttpApiE2E, RealServerSmoke)
{
    const char* base = std::getenv("GOCOOK_E2E_BASE");
    if (!base || !*base)
        GTEST_SKIP() << "未设置 GOCOOK_E2E_BASE，跳过真实服务端端到端测试";

    HttpGoCookApi api;
    api.setBaseUrl(QString::fromUtf8(base));
    api.setMaxRetries(2);
    api.setRetryDelay(50);

    // --- 1. 登录真实服务端 ---
    std::atomic<bool> done{false};
    bool ok = false;
    std::string err;
    std::string token;
    gocook::models::LoginRequest lr;
    lr.username = "testuser";
    lr.password = "test123";
    api.login(lr, [&](bool s, const gocook::models::LoginResponse& resp, const std::string& e) {
        ok = s;
        token = resp.token;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "登录超时";
    ASSERT_TRUE(ok) << "登录失败: " << err;
    ASSERT_FALSE(token.empty());
    api.setAuthToken(token);

    // --- 2. 空关键词搜索 → 真实 400，且只发一次 ---
    done = false;
    ok = false;
    int cbCount = 0;
    api.searchRecipes("", 1, 5, {},
                      [&](bool s, const gocook::models::PagedRecipes&, const std::string& e) {
                          ok = s;
                          err = e;
                          cbCount++;
                          done = true;
                      });
    ASSERT_TRUE(waitUntil(done)) << "搜索超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err, "keyword cannot be empty") << "应透传服务端 400 文案";
    EXPECT_EQ(cbCount, 1) << "业务 400 不得重试";

    // --- 3. 假 token → 真实 401 + unauthorized 信号 ---
    api.setAuthToken("fake_token_for_e2e");
    std::atomic<bool> unauthorizedCalled{false};
    QObject::connect(&api, &HttpGoCookApi::unauthorized, [&]() { unauthorizedCalled = true; });
    done = false;
    ok = true;
    api.getCurrentUser([&](bool s, const gocook::models::UserProfile&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "getCurrentUser 超时";
    EXPECT_FALSE(ok);
    EXPECT_TRUE(unauthorizedCalled.load()) << "真实 401 必须触发 unauthorized";
    EXPECT_EQ(err, "无效的访问令牌");

    // --- 4. 断网（指向无监听端口）→ 统一网络文案 ---
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:9"));
    done = false;
    ok = true;
    api.getCurrentUser([&](bool s, const gocook::models::UserProfile&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "断网请求超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err, "网络连接失败，请检查网络");
}
