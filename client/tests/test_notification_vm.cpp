// NotificationViewModel 测试
//
// 覆盖 isRefreshing 激活（refresh 置真 / 完成复位）与公告过滤路径：
//   T1  refresh() 立即置刷新态，加载完成后复位（isRefreshingChanged 序列 true→false）
//   T2  setCurrentType("system") 只走公告接口（通知接口零请求）；公告映射字段 is_read=true / type=system
//   T3  "全部"首页 = 公告 + 通知合并，按 createdAt 降序；未读数只计真实通知
//   T4  公告不可标读：markRead 直接跳过（无请求、无信号）
//   T5  普通通知 markRead：PATCH 一次 + markReadSuccess + 未读数与 is_read 同步更新
//   T6  公告删除仅本地移除：deleteSuccess + 列表移除 + 无 DELETE 请求
//   T7  markAllRead：仅公告列表时整体跳过；含真实通知时走接口并清零未读

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "HttpGoCookApi.h"
#include "NotificationViewModel.h"
#include <httplib/httplib.h>

namespace {

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
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return true;
}

// 负向断言用：驱动事件循环一小段时间（验证"不该发生的事没发生"）
void pumpEvents(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// 通知域桩服务：通知列表 / 公告列表 / 标读 / 全读 / 删除
// 固定数据（覆盖各路径的最小集）：
//   通知：301 未读(01-03) / 302 已读(01-01)；公告：901(01-02)
class NotificationStubServer {
public:
    struct ListReq {
        int page = 0;
        int size = 0;
        std::string type;
    };

    std::atomic<int> notificationsReqCount{0};
    std::atomic<int> announcementsReqCount{0};
    std::atomic<int> markReadReqCount{0};
    std::atomic<int> markAllReadReqCount{0};
    std::atomic<int> deleteReqCount{0};
    std::atomic<int> lastMarkReadId{-1};

    ListReq lastNotificationsReq()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return notifReqs.empty() ? ListReq{} : notifReqs.back();
    }

    NotificationStubServer()
    {
        svr.Get("/api/users/me/notifications", [this](const httplib::Request& req, httplib::Response& res) {
            notificationsReqCount++;
            {
                std::lock_guard<std::mutex> lk(reqMx);
                notifReqs.push_back({std::stoi(req.get_param_value("page")),
                                     std::stoi(req.get_param_value("size")),
                                     req.get_param_value("type")});
            }
            res.status = 200;
            res.set_content(
                R"({"pagination":{"page":1,"size":20,"total":2,"total_pages":1},)"
                R"("data":[)"
                R"({"id":301,"title":"点赞通知","content":"有人赞了你的菜谱","type":"like","is_read":false,"created_at":"2024-01-03T00:00:00Z"},)"
                R"({"id":302,"title":"评论通知","content":"有人评论了你的菜谱","type":"comment","is_read":true,"created_at":"2024-01-01T00:00:00Z"}]})",
                "application/json");
        });
        svr.Get("/api/announcements", [this](const httplib::Request&, httplib::Response& res) {
            announcementsReqCount++;
            res.status = 200;
            res.set_content(
                R"({"pagination":{"page":1,"size":20,"total":1,"total_pages":1},)"
                R"("data":[{"id":901,"title":"系统公告","content":"系统维护通知","created_at":"2024-01-02T00:00:00Z"}]})",
                "application/json");
        });
        svr.Patch(R"(/api/users/me/notifications/(\d+)/read)", [this](const httplib::Request& req, httplib::Response& res) {
            markReadReqCount++;
            lastMarkReadId = std::stoi(req.matches[1]);
            res.status = 200;
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        svr.Put("/api/users/me/notifications/read-all", [this](const httplib::Request&, httplib::Response& res) {
            markAllReadReqCount++;
            res.status = 200;
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        svr.Delete(R"(/api/users/me/notifications/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            deleteReqCount++;
            res.status = 200;
            res.set_content(R"({"message":"ok"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("NotificationStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running()
        // 显式暴露启动失败；同时消除"端口已绑定但尚未开始监听"的竞态窗口
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("NotificationStubServer: failed to start");
    }

    ~NotificationStubServer()
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
    std::mutex reqMx;   // 保护请求流水（服务端线程写入，测试线程读取）
    std::vector<ListReq> notifReqs;
};

class NotificationVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QStringLiteral("token-A"));
    }

    // 翻一次当前筛选的第一页并等待完成（完成标志 = isRefreshing 回落 false）
    bool refreshAndWait(NotificationViewModel& vm, int timeoutMs = 5000)
    {
        std::atomic<bool> done{false};
        auto conn = QObject::connect(&vm, &NotificationViewModel::isRefreshingChanged,
                                     [&]() { if (!vm.isRefreshing()) done = true; });
        vm.refresh();
        const bool ok = waitUntil(done, timeoutMs);
        QObject::disconnect(conn);
        return ok;
    }

    // 切换筛选（内含自动 refresh）并等待完成
    bool switchTypeAndWait(NotificationViewModel& vm, const QString& type, int timeoutMs = 5000)
    {
        std::atomic<bool> done{false};
        auto conn = QObject::connect(&vm, &NotificationViewModel::isRefreshingChanged,
                                     [&]() { if (!vm.isRefreshing()) done = true; });
        vm.setCurrentType(type);
        const bool ok = waitUntil(done, timeoutMs);
        QObject::disconnect(conn);
        return ok;
    }

    HttpGoCookApi api;
};

// ==================== T1：refresh 置刷新态、完成后复位 ====================
TEST_F(NotificationVmTest, refresh置刷新态并完成后复位)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    std::vector<bool> seq;   // 每次 isRefreshingChanged 时的当前值（主线程信号，无需加锁）
    std::atomic<bool> reset{false};
    QObject::connect(&vm, &NotificationViewModel::isRefreshingChanged,
                     [&]() {
                         seq.push_back(vm.isRefreshing());
                         if (!vm.isRefreshing()) reset = true;
                     });

    vm.refresh();
    EXPECT_TRUE(vm.isRefreshing()) << "refresh() 后应立即处于刷新态（QML 指示器据此显隐）";
    ASSERT_TRUE(waitUntil(reset)) << "刷新完成超时";
    EXPECT_FALSE(vm.isRefreshing()) << "加载完成后必须复位";
    ASSERT_EQ(seq.size(), 2u) << "预期恰好两次翻转：true（开始）→ false（完成）";
    EXPECT_TRUE(seq[0]);
    EXPECT_FALSE(seq[1]);
    EXPECT_EQ(stub.notificationsReqCount.load(), 1);
    EXPECT_EQ(stub.announcementsReqCount.load(), 1) << "\"全部\"首页应同时拉取公告";
}

// ==================== T2：system 筛选只走公告接口 ====================
TEST_F(NotificationVmTest, system筛选只走公告接口且映射字段正确)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(switchTypeAndWait(vm, QStringLiteral("system"))) << "system 筛选加载超时";

    EXPECT_EQ(stub.announcementsReqCount.load(), 1);
    EXPECT_EQ(stub.notificationsReqCount.load(), 0) << "system 筛选不得请求通知接口";
    ASSERT_EQ(vm.notifications().size(), 1);
    const auto item = vm.notifications().first().toMap();
    EXPECT_EQ(item["id"].toInt(), 901);
    EXPECT_TRUE(item["is_read"].toBool()) << "公告映射必须视为已读";
    EXPECT_EQ(item["type"].toString(), QStringLiteral("system"));
    EXPECT_EQ(vm.unreadCount(), 0) << "公告不计未读";
}

// ==================== T3："全部"首页合并 + 降序 + 未读只计真实通知 ====================
TEST_F(NotificationVmTest, 全部首页公告与通知合并按时间降序)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(refreshAndWait(vm)) << "刷新加载超时";

    EXPECT_EQ(stub.announcementsReqCount.load(), 1);
    EXPECT_EQ(stub.notificationsReqCount.load(), 1);
    EXPECT_TRUE(stub.lastNotificationsReq().type.empty()) << "\"全部\"请求不得携带 type 过滤";

    ASSERT_EQ(vm.notifications().size(), 3);
    const auto list = vm.notifications();
    EXPECT_EQ(list[0].toMap()["id"].toInt(), 301) << "最新（01-03 通知）应排最前";
    EXPECT_EQ(list[1].toMap()["id"].toInt(), 901) << "中间为 01-02 公告";
    EXPECT_EQ(list[2].toMap()["id"].toInt(), 302) << "最旧（01-01 通知）应在末位";
    EXPECT_EQ(vm.unreadCount(), 1) << "仅 301 未读；公告与 302 已读不计";
}

// ==================== T4：公告不可标读（直接跳过） ====================
TEST_F(NotificationVmTest, 公告markRead直接跳过)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(switchTypeAndWait(vm, QStringLiteral("system"))) << "system 筛选加载超时";

    std::atomic<bool> success{false};
    QObject::connect(&vm, &NotificationViewModel::markReadSuccess, [&](int) { success = true; });

    vm.markRead(901);
    pumpEvents(300);
    EXPECT_FALSE(success.load()) << "公告标读不应发成功信号";
    EXPECT_EQ(stub.markReadReqCount.load(), 0) << "公告标读不应发请求";
    EXPECT_EQ(vm.unreadCount(), 0);
}

// ==================== T5：普通通知 markRead 发请求并更新未读 ====================
TEST_F(NotificationVmTest, 普通通知markRead发请求并更新未读)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(refreshAndWait(vm)) << "刷新加载超时";
    ASSERT_EQ(vm.unreadCount(), 1);

    std::atomic<bool> success{false};
    int gotId = -1;
    QObject::connect(&vm, &NotificationViewModel::markReadSuccess,
                     [&](int id) { gotId = id; success = true; });

    vm.markRead(301);
    ASSERT_TRUE(waitUntil(success)) << "标读成功信号超时";
    EXPECT_EQ(gotId, 301);
    EXPECT_EQ(stub.markReadReqCount.load(), 1);
    EXPECT_EQ(stub.lastMarkReadId.load(), 301);
    EXPECT_EQ(vm.unreadCount(), 0) << "标读后未读数应重算为零";

    bool found = false;
    for (const auto& v : vm.notifications()) {
        if (v.toMap()["id"].toInt() == 301) {
            found = true;
            EXPECT_TRUE(v.toMap()["is_read"].toBool()) << "本地 is_read 应更新";
        }
    }
    EXPECT_TRUE(found);
}

// ==================== T6：公告删除仅本地移除（不发请求） ====================
TEST_F(NotificationVmTest, 公告删除仅本地移除不发请求)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(switchTypeAndWait(vm, QStringLiteral("system"))) << "system 筛选加载超时";

    std::atomic<bool> deleted{false};
    int gotId = -1;
    QObject::connect(&vm, &NotificationViewModel::deleteSuccess,
                     [&](int id) { gotId = id; deleted = true; });

    vm.deleteNotification(901);
    ASSERT_TRUE(waitUntil(deleted)) << "本地移除应立即发 deleteSuccess";
    EXPECT_EQ(gotId, 901);
    EXPECT_TRUE(vm.notifications().isEmpty());
    EXPECT_EQ(stub.deleteReqCount.load(), 0) << "公告删除不应发 DELETE 请求";
}

// ==================== T7：markAllRead 双分支（仅公告跳过 / 含通知清零） ====================
TEST_F(NotificationVmTest, markAllRead仅公告时跳过含通知时清零)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);
    ASSERT_TRUE(switchTypeAndWait(vm, QStringLiteral("system"))) << "system 筛选加载超时";

    std::atomic<bool> allDone{false};
    QObject::connect(&vm, &NotificationViewModel::markAllReadSuccess, [&]() { allDone = true; });

    // 分支一：列表只有公告 → 整体跳过（无请求、无信号）
    vm.markAllRead();
    pumpEvents(300);
    EXPECT_FALSE(allDone.load()) << "列表只有公告时应整体跳过";
    EXPECT_EQ(stub.markAllReadReqCount.load(), 0);

    // 分支二：切到"全部"（含真实通知）→ 走接口并清零未读
    ASSERT_TRUE(switchTypeAndWait(vm, QString())) << "切回\"全部\"加载超时";
    ASSERT_EQ(vm.unreadCount(), 1);
    allDone = false;
    vm.markAllRead();
    ASSERT_TRUE(waitUntil(allDone)) << "全部已读信号超时";
    EXPECT_EQ(stub.markAllReadReqCount.load(), 1);
    EXPECT_EQ(vm.unreadCount(), 0);
    for (const auto& v : vm.notifications()) {
        EXPECT_TRUE(v.toMap()["is_read"].toBool()) << "全读后本地所有条目 is_read=true";
    }
}

} // namespace
