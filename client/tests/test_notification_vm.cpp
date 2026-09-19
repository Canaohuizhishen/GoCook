// NotificationViewModel 测试（v2.23 水位模型）
//
// 覆盖双分类独立列表 / 未读汇总角标 / 「进入分类即已读」的水位上报与本地清零：
//   T1  汇总拉取：review/interaction/system 三值落位；失败静默保留旧值
//   T2  进入分类即已读：第一页成功 → 上报水位（type+maxId）+ 本地去点 + 该分类计数清零
//   T3  双分类独立：切换惰性加载、切回不重拉（缓存）、refresh 两类过期后重拉
//   T4  续页：翻页追加、hasMore 收敛、不重报水位
//   T5  删除已读条目：本地移除 + 计数不变
//   T6  删除未读条目：该分类计数 -1（汇总先于水位生效的窗口，下限 0）
//   T7  会话切换：在途响应作废（不落数据、不上报）；clearAll 全量归零
//
// 桩数据集：review=301..322、interaction=401..422（各 22 条，按 id 降序一页 20 条）

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
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

// 通知域桩服务：列表（可配延迟）/ 未读汇总（可配失败与载荷）/ 水位上报（记录流水）/ 删除
class NotificationStubServer {
public:
    std::atomic<int> listReqCount{0};
    std::atomic<int> listDelayMs{0};
    std::atomic<int> summaryReqCount{0};
    std::atomic<bool> summaryFail{false};
    std::atomic<int> summaryReview{2};
    std::atomic<int> summaryInteraction{2};
    std::atomic<bool> summarySystem{true};
    std::atomic<int> readStateReqCount{0};
    std::atomic<int> deleteReqCount{0};

    std::vector<std::pair<std::string, int>> readStatesCopy()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return readStates;
    }

    NotificationStubServer()
    {
        svr.Get("/api/users/me/notifications", [this](const httplib::Request& req, httplib::Response& res) {
            listReqCount++;
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            const std::string type = req.get_param_value("type");
            sleepIfNeeded(listDelayMs);

            // 数据集：review=301..322 / interaction=401..422（各 22 条，id 降序）
            const bool inter = (type == "interaction");
            const int base = inter ? 400 : 300;
            const int total = 22;
            const int from = (page - 1) * size;
            const int to = std::min(from + size, total);
            const int totalPages = (total + size - 1) / size;

            std::string arr = "[";
            for (int i = from; i < to; ++i) {
                if (i > from) arr += ",";
                const int id = base + total - i;   // i=0 → 最大 id
                arr += R"({"id":)" + std::to_string(id)
                     + R"(,"title":"通知)" + std::to_string(id)
                     + R"(","content":"内容","type":")" + type
                     + R"(","sub_type":null,"is_read":false,"created_at":"2026-09-01T00:00:00Z"})";
            }
            arr += "]";
            res.status = 200;
            res.set_content(
                R"({"pagination":{"page":)" + std::to_string(page)
                + R"(,"size":)" + std::to_string(size)
                + R"(,"total":)" + std::to_string(total)
                + R"(,"total_pages":)" + std::to_string(totalPages)
                + R"(},"data":)" + arr + "}",
                "application/json");
        });

        svr.Get("/api/users/me/notifications/unread-summary", [this](const httplib::Request&, httplib::Response& res) {
            summaryReqCount++;
            if (summaryFail.load()) {
                res.status = 500;
                res.set_content(R"({"error":"模拟汇总失败"})", "application/json");
                return;
            }
            res.status = 200;
            res.set_content(
                std::string("{\"review\":") + std::to_string(summaryReview.load())
                + ",\"interaction\":" + std::to_string(summaryInteraction.load())
                + ",\"system\":" + (summarySystem.load() ? "true" : "false") + "}",
                "application/json");
        });

        svr.Put("/api/users/me/notifications/read-state", [this](const httplib::Request& req, httplib::Response& res) {
            readStateReqCount++;
            const QJsonObject obj = QJsonDocument::fromJson(
                QByteArray::fromStdString(req.body)).object();
            {
                std::lock_guard<std::mutex> lk(reqMx);
                readStates.emplace_back(obj["type"].toString().toStdString(),
                                        obj["last_seen_id"].toInt());
            }
            res.status = 200;
            res.set_content(R"({"message":"已读水位已更新"})", "application/json");
        });

        svr.Delete(R"(/api/users/me/notifications/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            deleteReqCount++;
            res.status = 200;
            res.set_content(R"({"message":"通知已删除"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("NotificationStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running() 显式暴露启动失败
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
    void sleepIfNeeded(const std::atomic<int>& ms)
    {
        const int delay = ms.load();
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    httplib::Server svr;
    int port = 0;
    std::thread th;
    std::mutex reqMx;   // 保护 readStates（服务端线程写 / 测试线程读）
    std::vector<std::pair<std::string, int>> readStates;   // (type, last_seen_id) 按到达顺序
};

class NotificationVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QStringLiteral("token-A"));
    }

    HttpGoCookApi api;
};

// ==================== T1：汇总拉取与失败静默 ====================
TEST_F(NotificationVmTest, 汇总拉取与失败静默保留旧值)
{
    NotificationStubServer stub;
    stub.summaryReview = 3;
    stub.summaryInteraction = 1;
    stub.summarySystem = true;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    int summarySignals = 0;
    QObject::connect(&vm, &NotificationViewModel::unreadSummaryChanged, [&]() { summarySignals++; });

    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return vm.reviewUnread() == 3; })) << "汇总未落位";
    EXPECT_EQ(vm.interactionUnread(), 1);
    EXPECT_EQ(vm.unreadTotal(), 4) << "总数 = review + interaction（不含系统红点）";
    EXPECT_TRUE(vm.systemHasNew());
    EXPECT_EQ(stub.summaryReqCount.load(), 1);

    // 失败静默：保留旧值、不打断（无 errorOccurred 义务，角标不变）
    stub.summaryFail = true;
    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return stub.summaryReqCount.load() == 2; }));
    pumpEvents(100);
    EXPECT_EQ(vm.reviewUnread(), 3) << "失败必须保留旧值";
    EXPECT_EQ(vm.interactionUnread(), 1);
    EXPECT_TRUE(vm.systemHasNew());
}

// ==================== T2：进入分类即已读 ====================
TEST_F(NotificationVmTest, 进入分类即已读上报水位并本地清零)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    // 先取回汇总（review 未读 2）
    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return vm.reviewUnread() == 2; }));

    // 进入页面（refresh）：加载 review 第一页
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() {
        return vm.notifications().size() == 20 && !vm.isLoading();
    })) << "分类第一页加载超时";

    // 水位上报：type=review, last_seen_id=322（第一页最大 id）
    // （上报为 fire-and-forget：等桩收到，不能用 VM 状态推断）
    ASSERT_TRUE(waitUntil([&]() { return stub.readStateReqCount.load() == 1; })) << "水位上报未到达";
    const auto reqs = stub.readStatesCopy();
    ASSERT_EQ(reqs.size(), 1u);
    EXPECT_EQ(reqs[0].first, "review");
    EXPECT_EQ(reqs[0].second, 322);

    // 本地清零：分类计数归零 + 已加载条目未读点抹平
    EXPECT_EQ(vm.reviewUnread(), 0);
    for (const auto& v : vm.notifications())
        EXPECT_TRUE(v.toMap()["is_read"].toBool()) << "进入分类后已加载条目应本地去点";

    EXPECT_TRUE(vm.hasMore());
}

// ==================== T3：双分类独立（缓存 + 过期重拉） ====================
TEST_F(NotificationVmTest, 双分类独立切换不重拉刷新后重拉)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    vm.refresh();   // review 第一页（第 1 个请求）
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));
    EXPECT_EQ(vm.notifications().first().toMap()["id"].toInt(), 322);

    vm.setCurrentType(QStringLiteral("interaction"));   // 惰性加载（第 2 个请求）
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));
    EXPECT_EQ(vm.notifications().first().toMap()["id"].toInt(), 422);
    EXPECT_EQ(stub.listReqCount.load(), 2);

    vm.setCurrentType(QStringLiteral("review"));   // 切回：命中缓存，不重拉
    pumpEvents(150);
    EXPECT_EQ(stub.listReqCount.load(), 2) << "切回已加载分类不得重拉";
    EXPECT_EQ(vm.notifications().size(), 20);
    EXPECT_EQ(vm.notifications().first().toMap()["id"].toInt(), 322);

    vm.refresh();   // 重进/刷新：两类过期，当前分类重拉（第 3 个请求）
    ASSERT_TRUE(waitUntil([&]() { return stub.listReqCount.load() == 3 && !vm.isLoading(); }));
    EXPECT_EQ(vm.notifications().size(), 20);
}

// ==================== T4：续页追加、hasMore 收敛、不重报水位 ====================
TEST_F(NotificationVmTest, 续页追加且不重报水位)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));
    ASSERT_TRUE(waitUntil([&]() { return stub.readStateReqCount.load() == 1; })) << "水位上报未到达";

    vm.loadNextPage();
    ASSERT_TRUE(waitUntil([&]() {
        return vm.notifications().size() == 22 && !vm.isLoading();
    })) << "续页超时";
    EXPECT_FALSE(vm.hasMore());
    EXPECT_EQ(stub.readStateReqCount.load(), 1) << "续页不得重报水位";
    EXPECT_EQ(vm.notifications().last().toMap()["id"].toInt(), 301);
}

// ==================== T5：删除已读条目（计数不变） ====================
TEST_F(NotificationVmTest, 删除已读条目本地移除且计数不变)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    vm.refresh();   // 进入分类 → 全部视为已读
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));
    ASSERT_EQ(vm.reviewUnread(), 0);

    std::atomic<bool> deleted{false};
    int gotId = -1;
    QObject::connect(&vm, &NotificationViewModel::deleteSuccess,
                     [&](int id) { gotId = id; deleted = true; });

    vm.deleteNotification(305);
    ASSERT_TRUE(waitUntil(deleted)) << "删除成功信号超时";
    EXPECT_EQ(gotId, 305);
    EXPECT_EQ(stub.deleteReqCount.load(), 1);
    EXPECT_EQ(vm.reviewUnread(), 0) << "已读条目删除不改计数";

    for (const auto& v : vm.notifications())
        EXPECT_NE(v.toMap()["id"].toInt(), 305) << "删除后本地应移除该条目";
}

// ==================== T6：删除未读条目计数 -1 ====================
TEST_F(NotificationVmTest, 删除未读条目计数减一)
{
    NotificationStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    // 汇总：interaction 未读 2
    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return vm.interactionUnread() == 2; }));

    // 进入 interaction：本地清零（进入即已读）
    vm.setCurrentType(QStringLiteral("interaction"));
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));
    EXPECT_EQ(vm.interactionUnread(), 0);

    // 窗口：汇总先于水位上报生效 → 汇总值重新盖回 2
    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return vm.interactionUnread() == 2; }));

    // 续页带来未读条目（is_read=false，续页不去点）
    vm.loadNextPage();
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 22 && !vm.isLoading(); }));

    std::atomic<bool> deleted{false};
    QObject::connect(&vm, &NotificationViewModel::deleteSuccess, [&](int) { deleted = true; });
    vm.deleteNotification(401);   // 第二页未读条目
    ASSERT_TRUE(waitUntil(deleted)) << "删除成功信号超时";
    EXPECT_EQ(vm.interactionUnread(), 1) << "删除未读条目：该分类计数 -1";
}

// ==================== T7：会话切换在途作废 + clearAll 归零 ====================
TEST_F(NotificationVmTest, 会话切换在途作废与clearAll归零)
{
    NotificationStubServer stub;
    stub.listDelayMs = 300;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    NotificationViewModel vm(&api);

    vm.loadNotifications(1, 20);
    ASSERT_TRUE(vm.isLoading()) << "列表请求应在途";

    api.setToken(QStringLiteral("token-B"));   // 登出/换号

    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "过期响应后计数未配平";
    pumpEvents(100);
    EXPECT_TRUE(vm.notifications().isEmpty()) << "旧账号数据不得落入新界面";
    EXPECT_EQ(stub.readStateReqCount.load(), 0) << "过期响应不得上报水位";

    // clearAll：全量归零并各发信号
    NotificationStubServer stub2;
    api.setBaseUrl(QString::fromStdString(stub2.baseUrl()));
    api.setToken(QStringLiteral("token-A"));
    vm.refreshUnreadSummary();
    ASSERT_TRUE(waitUntil([&]() { return vm.reviewUnread() == 2; }));
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return vm.notifications().size() == 20 && !vm.isLoading(); }));

    int summaryChanged = 0, currentChanged = 0;
    QObject::connect(&vm, &NotificationViewModel::unreadSummaryChanged, [&]() { summaryChanged++; });
    QObject::connect(&vm, &NotificationViewModel::currentTypeChanged, [&]() { currentChanged++; });

    vm.clearAll();
    EXPECT_TRUE(vm.notifications().isEmpty());
    EXPECT_EQ(vm.reviewUnread(), 0);
    EXPECT_EQ(vm.interactionUnread(), 0);
    EXPECT_EQ(vm.unreadTotal(), 0);
    EXPECT_FALSE(vm.systemHasNew());
    EXPECT_EQ(vm.currentType(), QStringLiteral("review"));
    EXPECT_GE(summaryChanged, 1);
    EXPECT_GE(currentChanged, 1);
}

} // namespace
