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
    std::atomic<int> favoriteReqCount{0};        // POST 收藏（Interactive 挂起/重放计数）
    std::atomic<int> favoritesListReqCount{0};   // GET 收藏列表（Silent 拦截计数）

    StubServer()
    {
        // 与真实服务端 RecipeHandler::searchRecipes 完全一致的 400 响应
        svr.Get("/api/recipes/search", [this](const httplib::Request&, httplib::Response& res) {
            searchReqCount++;
            res.status = 400;
            res.set_content(R"({"error":"搜索关键词不能为空"})", "application/json");
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
        // 登录守卫测试用：收藏写操作成功响应 + 收藏列表成功响应
        svr.Post("/api/recipes/1/favorite", [this](const httplib::Request&, httplib::Response& res) {
            favoriteReqCount++;
            res.status = 200;
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        svr.Get("/api/users/me/favorites", [this](const httplib::Request&, httplib::Response& res) {
            favoritesListReqCount++;
            res.status = 200;
            res.set_content(R"({"pagination":{"page":1,"size":20,"total":0,"total_pages":0},"data":[]})", "application/json");
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
        api.setToken(QString()); // 每个用例从"未登录"开始（避免上一个用例的 token 污染）
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
    EXPECT_EQ(err.toStdString(), "搜索关键词不能为空");
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
    // 模拟"已登录但 token 失效"：无 token 时 Interactive 请求会被登录守卫拦截（不发请求，见 Guard 用例），
    // 只有带 token 才能命中服务端 401 路径
    api.setToken(QStringLiteral("stale-token"));

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
    // 模拟"已登录但 token 失效"（无 token 会被登录守卫拦截，见 Guard 用例）
    api.setToken(QStringLiteral("stale-token"));

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

// ==================== T4.1：Interactive 未登录 → 挂起 + authRequired，不发请求 ====================
TEST_F(HttpApiTest, InteractiveGuest_SuspendsAndEmitsAuthRequired)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> authRequiredCalled{false};
    QObject::connect(&api, &HttpGoCookApi::authRequired, [&]() { authRequiredCalled = true; });

    std::atomic<bool> done{false}; // 挂起期间回调不应触发
    api.toggleFavorite(1, 1, std::nullopt, [&](bool, const std::string&) { done = true; });

    // 给事件循环一点时间：若误发请求/误回调，这里会捕捉到
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    EXPECT_TRUE(authRequiredCalled.load()) << "Interactive 未登录必须触发 authRequired";
    EXPECT_EQ(stub.favoriteReqCount.load(), 0) << "挂起期间不得发出网络请求";
    EXPECT_FALSE(done.load()) << "挂起期间回调不得触发";

    // 清理挂起队列，避免污染后续用例
    api.cancelAuthQueue();
    // 断开信号连接：lambda 捕获本用例栈，残留连接会在后续用例 emit 时触发悬垂 UB
    QObject::disconnect(&api, &HttpGoCookApi::authRequired, nullptr, nullptr);
}

// ==================== T4.2：登录成功后自动重放挂起请求 ====================
TEST_F(HttpApiTest, InteractiveGuest_ReplayAfterLogin)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> authRequiredCalled{false};
    QObject::connect(&api, &HttpGoCookApi::authRequired, [&]() { authRequiredCalled = true; });

    std::atomic<bool> done{false};
    bool ok = false;
    std::string err;
    api.toggleFavorite(1, 1, std::nullopt, [&](bool s, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_TRUE(authRequiredCalled.load());
    EXPECT_EQ(stub.favoriteReqCount.load(), 0);
    EXPECT_FALSE(done.load());

    // 模拟登录成功：setToken → tokenChanged → 自动重放
    api.setToken(QStringLiteral("valid-token"));

    ASSERT_TRUE(waitUntil(done)) << "登录后重放超时";
    EXPECT_TRUE(ok);
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(stub.favoriteReqCount.load(), 1) << "重放应恰好发出 1 次请求";
    // 断开信号连接（lambda 捕获本用例栈，残留会在后续用例 emit 时触发悬垂 UB）
    QObject::disconnect(&api, &HttpGoCookApi::authRequired, nullptr, nullptr);
}

// ==================== T4.3：登录页取消 → 挂起请求按"请先登录"失败 ====================
TEST_F(HttpApiTest, InteractiveGuest_CancelFailsWithLoginRequired)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> done{false};
    bool ok = true;
    std::string err;
    api.toggleFavorite(1, 1, std::nullopt, [&](bool s, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    api.cancelAuthQueue();

    ASSERT_TRUE(waitUntil(done)) << "取消后回调超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err, "请先登录");
    EXPECT_EQ(stub.favoriteReqCount.load(), 0) << "取消后不得发出请求";
}

// ==================== T4.4：Silent 未登录 → 不发请求、不发 authRequired、静默失败 ====================
TEST_F(HttpApiTest, SilentGuest_FailsWithoutRequestOrSignal)
{
    StubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    std::atomic<bool> authRequiredCalled{false};
    QObject::connect(&api, &HttpGoCookApi::authRequired, [&]() { authRequiredCalled = true; });

    std::atomic<bool> done{false};
    bool ok = true;
    std::string err;
    api.getFavorites(1, 20, "", [&](bool s, const gocook::models::PagedFavorites&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });

    ASSERT_TRUE(waitUntil(done)) << "回调超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err, "请先登录");
    EXPECT_EQ(stub.favoritesListReqCount.load(), 0) << "Silent 未登录不得发出请求";
    EXPECT_FALSE(authRequiredCalled.load()) << "Silent 不触发登录页";
    QObject::disconnect(&api, &HttpGoCookApi::authRequired, nullptr, nullptr);
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
    EXPECT_EQ(err, "搜索关键词不能为空") << "应透传服务端 400 文案";
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

    // --- 5. 游客模式全链路（登录守卫 + 挂起 + 重放，真实服务端） ---
    api.setBaseUrl(QString::fromUtf8(base));
    api.setAuthToken("");   // 回到游客（无 token）

    // 5.1 游客浏览公开接口：菜谱列表 + 详情（可选认证，无 token 也成功）
    done = false;
    ok = false;
    api.getPublicRecipes(1, 3, {}, [&](bool s, const gocook::models::PagedRecipes&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "游客浏览列表超时";
    EXPECT_TRUE(ok) << "游客浏览公开列表失败: " << err;

    done = false;
    ok = false;
    api.getRecipeDetail(1, [&](bool s, const gocook::models::RecipeDetail&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "游客浏览详情超时";
    EXPECT_TRUE(ok) << "游客浏览公开详情失败: " << err;

    // 5.2 游客触发写操作（Interactive）→ 挂起 + authRequired，不发出请求
    std::atomic<bool> authRequiredCalled{false};
    QObject::connect(&api, &HttpGoCookApi::authRequired, [&]() { authRequiredCalled = true; });
    done = false;
    ok = false;
    api.toggleFavorite(1, std::nullopt, std::nullopt,
                       [&](bool s, const std::string& e) { ok = s; err = e; done = true; });
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    EXPECT_TRUE(authRequiredCalled.load()) << "游客写操作必须触发 authRequired";
    EXPECT_FALSE(done.load()) << "挂起期间回调不得触发";

    // 5.3 登录成功 → tokenChanged 自动重放挂起请求 → 收藏成功
    api.setAuthToken(token);
    ASSERT_TRUE(waitUntil(done, 15000)) << "登录后重放超时";
    EXPECT_TRUE(ok) << "重放后的收藏应成功: " << err;

    // 5.4 游客 Silent 接口：静默失败"请先登录"，不触发 authRequired
    authRequiredCalled = false;
    QObject::disconnect(&api, &HttpGoCookApi::authRequired, nullptr, nullptr);
    QObject::connect(&api, &HttpGoCookApi::authRequired, [&]() { authRequiredCalled = true; });
    done = false;
    api.setAuthToken("");   // 再次回到游客
    api.getFavorites(1, 20, "", [&](bool s, const gocook::models::PagedFavorites&, const std::string& e) {
        ok = s;
        err = e;
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "游客 Silent 请求超时";
    EXPECT_FALSE(ok);
    EXPECT_EQ(err, "请先登录") << "Silent 未登录应静默失败";
    EXPECT_FALSE(authRequiredCalled.load()) << "Silent 不触发登录页";
    QObject::disconnect(&api, &HttpGoCookApi::authRequired, nullptr, nullptr);
}

// ==================== 整改 N5：库存编辑链路 PUT /api/inventory/:id E2E ====================
// 覆盖 Router/Handler 层的 HTTP 语义：200 整行替换、409 唯一冲突（数据不变）、404 不存在。
// 依赖真实服务端 + 种子账号 testuser/test123（与 RealServerSmoke 同门，未设 GOCOOK_E2E_BASE 即跳过）。
TEST(HttpApiE2E, InventoryPutEdit)
{
    const char* base = std::getenv("GOCOOK_E2E_BASE");
    if (!base || !*base)
        GTEST_SKIP() << "未设置 GOCOOK_E2E_BASE，跳过真实服务端端到端测试";

    HttpGoCookApi api;
    api.setBaseUrl(QString::fromUtf8(base));
    api.setMaxRetries(2);
    api.setRetryDelay(50);

    // --- 0. 登录 + 清场（专用前缀 E2E_ 行，幂等） ---
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
    api.setAuthToken(token);

    const auto cleanup = [&]() {
        done = false;
        api.getInventory(1, 100, "", [&](bool s, const gocook::models::PagedInventory& data, const std::string& e) {
            ok = s;
            err = e;
            if (s) {
                for (const auto& item : data.data) {
                    if (item.ingredient_name.rfind("E2E_", 0) == 0) {
                        std::atomic<bool> delDone{false};
                        api.deleteInventoryItem(item.id, [&](bool ds, const std::string&) { delDone = true; });
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                        while (!delDone.load())
                            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                    }
                }
            }
            done = true;
        });
        waitUntil(done);   // 阻塞等清场完成（回调内不放置断言宏，避免 void 返回冲突）
    };
    cleanup();
    ASSERT_TRUE(ok) << "清场失败: " << err;

    const std::string nameA = "E2E_料酒";
    const std::string nameB = "E2E_土豆";

    // --- 1. POST 同名同单位两次 → 累加（45），返回行 id ---
    gocook::models::UpsertInventoryRequest postA;
    postA.ingredient_name = nameA;
    postA.quantity = 15.0;
    postA.unit = "毫升";
    int idA = -1;
    done = false;
    ok = false;
    api.upsertInventory(postA, [&](bool s, int id, const std::string& e) { ok = s; idA = id; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "POST A 超时";
    ASSERT_TRUE(ok) << "POST A 失败: " << err;
    ASSERT_GT(idA, 0);

    postA.quantity = 30.0;
    done = false;
    ok = false;
    api.upsertInventory(postA, [&](bool s, int id, const std::string& e) { ok = s; idA = id; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "POST A(2) 超时";
    ASSERT_TRUE(ok) << "POST A(2) 失败: " << err;

    // --- 2. PUT 改名撞本人另一条同名同单位行 → 409，且原行数据不变 ---
    gocook::models::UpsertInventoryRequest postB;
    postB.ingredient_name = nameB;
    postB.quantity = 200.0;
    postB.unit = "克";
    int idB = -1;
    done = false;
    ok = false;
    api.upsertInventory(postB, [&](bool s, int id, const std::string& e) { ok = s; idB = id; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "POST B 超时";
    ASSERT_TRUE(ok) << "POST B 失败: " << err;
    ASSERT_GT(idB, 0);

    gocook::models::UpsertInventoryRequest clash;
    clash.ingredient_name = nameB;   // 撞 idB 那行（同名同单位）
    clash.quantity = 1.0;
    clash.unit = "克";
    done = false;
    ok = true;
    api.updateInventoryItem(idA, clash, [&](bool s, const std::string& e) { ok = s; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "PUT 冲突超时";
    EXPECT_FALSE(ok) << "改名撞唯一约束应 409 失败";
    EXPECT_NE(err.find("同名同单位"), std::string::npos) << "409 文案不符: " << err;

    // --- 3. PUT 不存在的 id → 404（服务端 ErrorHelper 对 404/401 统一归一为通用文案，
    //          与 DELETE 等接口一致——细节仅入服务端日志；409 才透传具体冲突文案） ---
    done = false;
    ok = true;
    api.updateInventoryItem(999999999, postB, [&](bool s, const std::string& e) { ok = s; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "PUT 404 超时";
    EXPECT_FALSE(ok) << "不存在条目应 404";
    EXPECT_EQ(err, "请求的资源不存在") << "404 应走服务端通用文案（ErrorHelper 归一策略）: " << err;

    // --- 4. 回读：idA 行未被 409 改动（仍是 45 毫升 E2E_料酒） ---
    done = false;
    ok = false;
    bool foundA = false;
    double qtyA = 0.0;
    api.getInventory(1, 100, "", [&](bool s, const gocook::models::PagedInventory& data, const std::string& e) {
        ok = s;
        err = e;
        if (s) {
            for (const auto& item : data.data) {
                if (item.id == idA) {
                    foundA = true;
                    qtyA = item.quantity;
                }
            }
        }
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "回读超时";
    ASSERT_TRUE(ok) << "回读失败: " << err;
    EXPECT_TRUE(foundA) << "idA 行应仍存在";
    EXPECT_DOUBLE_EQ(qtyA, 45.0) << "409 后数据必须不变（两次 POST 累加值）";

    // --- 5. PUT 合法编辑 = 按 id 整行替换（改名 200→250 克，非累加） ---
    gocook::models::UpsertInventoryRequest edit;
    edit.ingredient_name = "E2E_土豆改";
    edit.quantity = 250.0;
    edit.unit = "克";
    done = false;
    ok = false;
    api.updateInventoryItem(idB, edit, [&](bool s, const std::string& e) { ok = s; err = e; done = true; });
    ASSERT_TRUE(waitUntil(done)) << "PUT 编辑超时";
    ASSERT_TRUE(ok) << "PUT 编辑失败: " << err;

    // 回读验证：旧行 E2E_土豆 消失、新行 E2E_土豆改 = 250（替换而非 200+250 累加）
    done = false;
    ok = false;
    bool oldBGone = true;
    bool newBPresent = false;
    double qtyNewB = 0.0;
    api.getInventory(1, 100, "", [&](bool s, const gocook::models::PagedInventory& data, const std::string& e) {
        ok = s;
        err = e;
        if (s) {
            for (const auto& item : data.data) {
                if (item.ingredient_name == nameB) oldBGone = false;
                if (item.ingredient_name == "E2E_土豆改") { newBPresent = true; qtyNewB = item.quantity; }
            }
        }
        done = true;
    });
    ASSERT_TRUE(waitUntil(done)) << "编辑回读超时";
    ASSERT_TRUE(ok) << "编辑回读失败: " << err;
    EXPECT_TRUE(oldBGone) << "整行替换后旧名不应残留";
    EXPECT_TRUE(newBPresent) << "替换后新行应存在";
    EXPECT_DOUBLE_EQ(qtyNewB, 250.0) << "编辑=替换语义：数量应为 250 而非 200+250";

    // --- 6. 清场 ---
    cleanup();
    ASSERT_TRUE(ok) << "收尾清场失败: " << err;
}
