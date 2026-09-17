// test_auth_vm.cpp —— AuthViewModel 自动登录（checkAutoLogin）的会话判定契约
//
// 覆盖自动登录校验的三态判定（断网/服务器繁忙启动不得误删凭证与库存快照）：
//   A1 瞬时失败（网络层错误，连接拒绝）→ 保留凭证 + 快照 + token，乐观登录进入主界面
//   A2 瞬时失败（503 服务器繁忙）      → 同上
//   A3 服务端明确拒绝（401 无效令牌）  → 清理凭证与快照，保持登出
//   A4 成功路径                        → loggedIn + 资料字段填充
//   A5 上传暂存 → 保存绑定             → 上传不改已保存资料；PUT 携带 avatar_url；pending 清空
//   A6 上传暂存 → 放弃回滚             → 本地 pending 立即清空；DELETE 暂存送达；已保存资料不变
//   A7 重传失败不丢旧暂存              → 失败保持 pending；新上传成功才丢弃旧 URL
//   A8 会话结束统一信号                → 登录后登出：sessionEnded 恰一次；游客/重复登出不发
//
// 判定机制：校验窗口内的 401 由 HttpGoCookApi 的 unauthorizedHandler 标记——
// 不经字符串匹配、不改任何用户可见文案；本文件端到端锁定该机制（含回调顺序不变量：
// sendRaw 先触发 unauthorizedHandler 再回调业务结果）。

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSqlDatabase>
#include <QString>
#include <QTemporaryDir>

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
    std::atomic<int> avatarUploadReqCount{0};     // A5/A6：POST 头像暂存计数
    std::atomic<int> profilePutReqCount{0};       // A5：PUT 资料计数
    std::atomic<bool> profilePutHasAvatarUrl{false}; // A5：PUT 体携带暂存 avatar_url
    std::atomic<int> discardAvatarReqCount{0};    // A6：DELETE 暂存头像计数
    std::atomic<int> avatarUploadSeq{0};          // A7：上传序号（每次成功上传返回唯一 URL）
    std::atomic<bool> avatarUploadFail{false};    // A7：模拟上传失败
    std::atomic<bool> discardBodyHasFirstUrl{false};  // A7：删除体含首个暂存 URL（黏性）
    std::atomic<bool> discardBodyHasSecondUrl{false}; // A7：删除体含第二次暂存 URL（黏性）

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

        // A5/A6/A7 用：头像暂存上传（首次返回固定暂存 URL 与 PUT 响应保持一致；
        // A7 再传返回递增 URL；avatarUploadFail 置位时模拟失败）
        svr.Post("/api/users/me/avatar", [this](const httplib::Request&, httplib::Response& res) {
            avatarUploadReqCount++;
            if (avatarUploadFail.load()) {
                res.status = 500;
                res.set_content(R"({"error":"服务器内部错误，请稍后重试"})", "application/json");
                return;
            }
            const int n = ++avatarUploadSeq;
            const std::string url = n == 1
                ? "/uploads/avatars/user_1_777.jpg"
                : "/uploads/avatars/user_1_777_" + std::to_string(n) + ".jpg";
            res.status = 200;
            res.set_content("{\"avatar_url\":\"" + url + "\"}", "application/json");
        });
        // A5 用：保存资料（断言 PUT 体携带 avatar_url；响应回显绑定后的资料）
        svr.Put("/api/users/me/profile", [this](const httplib::Request& req, httplib::Response& res) {
            profilePutReqCount++;
            profilePutHasAvatarUrl =
                req.body.find("avatar_url") != std::string::npos &&
                req.body.find("/uploads/avatars/user_1_777.jpg") != std::string::npos;
            res.status = 200;
            res.set_content(R"({"id":1,"username":"tester","display_name":"测试","email":"t@test.com",)"
                            R"("phone":"13800000000","avatar_url":"/uploads/avatars/user_1_777.jpg",)"
                            R"("preferences_complete":true,"created_at":"2026-01-01"})",
                            "application/json");
        });
        // A6/A7 用：放弃暂存头像（黏性记录删除体指向哪个暂存 URL）
        svr.Delete("/api/users/me/avatar", [this](const httplib::Request& req, httplib::Response& res) {
            discardAvatarReqCount++;
            if (req.body.find("/uploads/avatars/user_1_777.jpg") != std::string::npos)
                discardBodyHasFirstUrl = true;
            if (req.body.find("user_1_777_2") != std::string::npos)
                discardBodyHasSecondUrl = true;
            res.status = 200;
            res.set_content(R"({"message":"已放弃未保存的头像"})", "application/json");
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

// ==================== A5：上传仅暂存，保存才绑定 ====================
TEST_F(AuthVmTest, 上传暂存保存后绑定生效)
{
    UsersMeStubServer stub;
    stub.mode = 0;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";
    ASSERT_TRUE(vm.loggedIn());

    // 造一个本地图片文件供上传
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile img(dir.filePath("avatar.png"));
    ASSERT_TRUE(img.open(QIODevice::WriteOnly));
    img.write("fake-png-bytes");
    img.close();

    // 上传：仅暂存——已保存资料不得被改动
    vm.uploadAvatar(img.fileName());
    ASSERT_TRUE(waitUntil([&]() { return vm.hasPendingAvatar(); })) << "暂存回调超时";
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/a.png"))
        << "上传不得直接改写已保存头像（两阶段：保存才绑定）";
    EXPECT_EQ(stub.avatarUploadReqCount.load(), 1);

    // 保存：携带暂存 avatar_url，绑定生效并清空 pending
    bool saved = false;
    QObject::connect(&vm, &AuthViewModel::profileSaved, [&]() { saved = true; });
    vm.saveProfile(QStringLiteral("测试"), QStringLiteral("t@test.com"), QStringLiteral("13800000000"));
    ASSERT_TRUE(waitUntil([&]() { return saved; })) << "保存回调超时";

    EXPECT_TRUE(stub.profilePutHasAvatarUrl.load()) << "保存请求必须携带暂存 avatar_url 完成绑定";
    EXPECT_FALSE(vm.hasPendingAvatar());
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/avatars/user_1_777.jpg"))
        << "保存成功后已保存资料更新为绑定后的头像";
}

// ==================== A6：上传暂存，返回放弃即回滚 ====================
TEST_F(AuthVmTest, 返回放弃暂存回滚且删除送达)
{
    UsersMeStubServer stub;
    stub.mode = 0;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";
    ASSERT_TRUE(vm.loggedIn());

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile img(dir.filePath("avatar.png"));
    ASSERT_TRUE(img.open(QIODevice::WriteOnly));
    img.write("fake-png-bytes");
    img.close();

    vm.uploadAvatar(img.fileName());
    ASSERT_TRUE(waitUntil([&]() { return vm.hasPendingAvatar(); })) << "暂存回调超时";

    // "返回"：本地立即回滚（pending 同步清空），服务端删除随后送达
    vm.discardPendingAvatar();
    EXPECT_FALSE(vm.hasPendingAvatar()) << "放弃必须本地立即生效（UI 先回滚）";
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/a.png")) << "已保存资料不受影响";
    ASSERT_TRUE(waitUntil([&]() { return stub.discardAvatarReqCount.load() == 1; }))
        << "放弃请求应送达服务端";
}

// ==================== A7：重传失败不丢旧暂存，新上传成功才丢弃 ====================
TEST_F(AuthVmTest, 重传失败保留旧暂存成功后才丢弃)
{
    UsersMeStubServer stub;
    stub.mode = 0;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";
    ASSERT_TRUE(vm.loggedIn());

    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QFile img(dir.filePath("avatar.png"));
    ASSERT_TRUE(img.open(QIODevice::WriteOnly));
    img.write("fake-png-bytes");
    img.close();

    // 首次上传成功：暂存 A（不丢弃任何东西）
    vm.uploadAvatar(img.fileName());
    ASSERT_TRUE(waitUntil([&]() { return vm.hasPendingAvatar(); })) << "首次暂存回调超时";
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/a.png")) << "上传不得改写已保存资料";
    EXPECT_EQ(stub.avatarUploadReqCount.load(), 1);
    EXPECT_EQ(stub.discardAvatarReqCount.load(), 0);

    // 重传 B 失败：旧暂存 A 必须保留（本地不清空、不发丢弃请求——修复前此处会丢 A）
    bool uploadFailed = false;
    QObject::connect(&vm, &AuthViewModel::avatarUploadFailed, [&]() { uploadFailed = true; });
    stub.avatarUploadFail = true;
    vm.uploadAvatar(img.fileName());
    ASSERT_TRUE(waitUntil([&]() { return uploadFailed; })) << "失败回调超时";
    EXPECT_TRUE(vm.hasPendingAvatar()) << "重传失败不得清空旧暂存（预览与保存语义保持一致）";
    EXPECT_EQ(stub.avatarUploadReqCount.load(), 2);
    EXPECT_EQ(stub.discardAvatarReqCount.load(), 0) << "失败的重传不得丢弃旧暂存";

    // 重传 C 成功：此刻才丢弃旧暂存 A，pending 切换为新 URL
    stub.avatarUploadFail = false;
    vm.uploadAvatar(img.fileName());
    ASSERT_TRUE(waitUntil([&]() { return stub.discardAvatarReqCount.load() == 1; }))
        << "新上传成功后应丢弃旧暂存";
    EXPECT_TRUE(stub.discardBodyHasFirstUrl.load()) << "被丢弃的应是首个暂存 URL";
    EXPECT_FALSE(stub.discardBodyHasSecondUrl.load()) << "新暂存不得被误丢";
    EXPECT_EQ(vm.profileAvatarUrl(), QStringLiteral("/uploads/a.png")) << "绑定仍待保存";

    // pending 已切换为新 URL：放弃它，确认删除体指向第二次暂存
    vm.discardPendingAvatar();
    EXPECT_FALSE(vm.hasPendingAvatar());
    ASSERT_TRUE(waitUntil([&]() { return stub.discardAvatarReqCount.load() == 2; }));
    EXPECT_TRUE(stub.discardBodyHasSecondUrl.load()) << "pending 应已切换为新上传的 URL";
}

// ==================== A8：sessionEnded —— 会话结束的统一出口 ====================
TEST_F(AuthVmTest, 会话结束信号登出恰一次游客与重复登出不再发)
{
    UsersMeStubServer stub;
    stub.mode = 0; // 200
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AuthViewModel vm(&api, nullptr, testDb);
    std::atomic<int> sessionEndedCount{0};
    std::atomic<int> logoutFinishedCount{0};
    QObject::connect(&vm, &AuthViewModel::sessionEnded, [&]() { sessionEndedCount++; });
    QObject::connect(&vm, &AuthViewModel::logoutFinished, [&]() { logoutFinishedCount++; });

    vm.checkAutoLogin();
    ASSERT_TRUE(waitUntil([&]() { return !vm.initialLoading(); })) << "校验回调超时";
    ASSERT_TRUE(vm.loggedIn());

    // 登出：登录态 true→false —— sessionEnded 与 logoutFinished 各恰一次
    vm.logout();
    EXPECT_FALSE(vm.loggedIn());
    EXPECT_EQ(sessionEndedCount.load(), 1) << "登出必须发且仅发一次 sessionEnded";
    EXPECT_EQ(logoutFinishedCount.load(), 1);

    // 游客态重复登出：已无会话可结束，不得再发
    vm.logout();
    EXPECT_EQ(sessionEndedCount.load(), 1) << "重复登出不得重复发 sessionEnded";
}

} // namespace
