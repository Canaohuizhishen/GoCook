// AnnouncementViewModel 测试（v2.23 系统通知独立页）
//
// 覆盖公告分页列表与「进入系统通知页即已读」的水位上报：
//   A1  进入页面（refresh）：第一页加载成功 → 发 announcementsSeen + 上报水位（maxId）；重复进入不重报
//   A2  续页：追加、hasMore 收敛、不重报
//   A3  加载失败：errorOccurred、不发 seen、不上报
//   A4  会话切换：在途响应作废（不落数据、不发 seen）
//   A5  clearAll：清空并重置已上报水位（此后 refresh 可重新上报）
//   A6  预览拉取：不发 seen、不上报水位、不动列表（三横条预览专用）
//   A7  预览失败静默保留旧值
//   A8  clearAll 清空预览值
//   A9  隐藏公告（"删除"）：同会话刷新/续页不复活，clearAll 恢复
//
// 桩数据集：公告 id 901..922（22 条，id 降序一页 20 条）

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

#include "AnnouncementViewModel.h"
#include "HttpGoCookApi.h"
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

// 负向断言用：驱动事件循环一小段时间
void pumpEvents(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// 公告域桩服务：公告列表（可配延迟/失败）/ 公告水位上报（记录流水）
class AnnouncementStubServer {
public:
    std::atomic<int> listReqCount{0};
    std::atomic<int> listDelayMs{0};
    std::atomic<bool> listFail{false};
    std::atomic<int> readStateReqCount{0};

    std::vector<int> seenIdsCopy()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return seenIds;
    }

    AnnouncementStubServer()
    {
        svr.Get("/api/announcements", [this](const httplib::Request& req, httplib::Response& res) {
            listReqCount++;
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            sleepIfNeeded(listDelayMs);
            if (listFail.load()) {
                res.status = 500;
                res.set_content(R"({"error":"模拟失败"})", "application/json");
                return;
            }

            // 数据集：公告 id 901..922（22 条，id 降序）
            const int total = 22;
            const int base = 900;
            const int from = (page - 1) * size;
            const int to = std::min(from + size, total);
            const int totalPages = (total + size - 1) / size;

            std::string arr = "[";
            for (int i = from; i < to; ++i) {
                if (i > from) arr += ",";
                const int id = base + total - i;   // i=0 → 922
                arr += R"({"id":)" + std::to_string(id)
                     + R"(,"title":"公告)" + std::to_string(id)
                     + R"(","content":"内容","created_at":"2026-09-01T00:00:00Z"})";
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

        svr.Put("/api/users/me/announcements/read-state", [this](const httplib::Request& req, httplib::Response& res) {
            readStateReqCount++;
            const QJsonObject obj = QJsonDocument::fromJson(
                QByteArray::fromStdString(req.body)).object();
            {
                std::lock_guard<std::mutex> lk(reqMx);
                seenIds.push_back(obj["last_seen_id"].toInt());
            }
            res.status = 200;
            res.set_content(R"({"message":"已读水位已更新"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("AnnouncementStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running() 显式暴露启动失败
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("AnnouncementStubServer: failed to start");
    }

    ~AnnouncementStubServer()
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
    std::mutex reqMx;   // 保护 seenIds（服务端线程写 / 测试线程读）
    std::vector<int> seenIds;   // 水位上报流水（last_seen_id 按到达顺序）
};

class AnnouncementVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QStringLiteral("token-A"));
    }

    HttpGoCookApi api;
};

// ==================== A1：进入上报 + 防重复 ====================
TEST_F(AnnouncementVmTest, 进入上报水位且重复进入不重报)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    int seenSignals = 0;
    QObject::connect(&vm, &AnnouncementViewModel::announcementsSeen, [&]() { seenSignals++; });

    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() {
        return vm.announcements().size() == 20 && !vm.isLoading();
    })) << "公告第一页加载超时";

    EXPECT_GE(seenSignals, 1) << "第一页加载成功应发 announcementsSeen";
    ASSERT_TRUE(waitUntil([&]() { return stub.readStateReqCount.load() == 1; })) << "水位上报未到达";
    pumpEvents(150);   // 等上报成功回执落账（本地防重记账以成功为准），保证"不重报"断言确定性
    auto ids = stub.seenIdsCopy();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], 922) << "上报应为第一页最大公告 id";
    EXPECT_TRUE(vm.hasMore());

    // 映射字段抽查：详情页复用通知详情组件（type=system）
    const auto first = vm.announcements().first().toMap();
    EXPECT_EQ(first["id"].toInt(), 922);
    EXPECT_EQ(first["type"].toString(), QStringLiteral("system"));

    // 重复进入（refresh 再次第一页）：maxId 未超过已上报值 → 不重报
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading() && vm.announcements().size() == 20; }));
    pumpEvents(100);
    EXPECT_EQ(stub.readStateReqCount.load(), 1) << "同一水位不得重报";
}

// ==================== A2：续页不重报 ====================
TEST_F(AnnouncementVmTest, 续页追加且不重报)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return vm.announcements().size() == 20 && !vm.isLoading(); }));

    vm.loadNextPage();
    ASSERT_TRUE(waitUntil([&]() {
        return vm.announcements().size() == 22 && !vm.isLoading();
    })) << "续页超时";
    EXPECT_FALSE(vm.hasMore());
    EXPECT_EQ(stub.readStateReqCount.load(), 1) << "续页不得重报水位";
    EXPECT_EQ(vm.announcements().last().toMap()["id"].toInt(), 901);
}

// ==================== A3：加载失败 ====================
TEST_F(AnnouncementVmTest, 加载失败上报错误不发seen)
{
    AnnouncementStubServer stub;
    stub.listFail = true;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    std::atomic<bool> errorSeen{false};
    int seenSignals = 0;
    QObject::connect(&vm, &AnnouncementViewModel::errorOccurred, [&](const QString&) { errorSeen = true; });
    QObject::connect(&vm, &AnnouncementViewModel::announcementsSeen, [&]() { seenSignals++; });

    vm.refresh();
    ASSERT_TRUE(waitUntil(errorSeen)) << "失败应上抛 errorOccurred";
    pumpEvents(100);
    EXPECT_EQ(seenSignals, 0) << "失败不得发 announcementsSeen";
    EXPECT_EQ(stub.readStateReqCount.load(), 0) << "失败不得上报水位";
    EXPECT_TRUE(vm.announcements().isEmpty());
    EXPECT_FALSE(vm.isLoading());
    EXPECT_FALSE(vm.isRefreshing());
}

// ==================== A4：会话切换在途作废 ====================
TEST_F(AnnouncementVmTest, 会话切换在途响应作废)
{
    AnnouncementStubServer stub;
    stub.listDelayMs = 300;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    int seenSignals = 0;
    QObject::connect(&vm, &AnnouncementViewModel::announcementsSeen, [&]() { seenSignals++; });

    vm.refresh();
    ASSERT_TRUE(vm.isLoading()) << "公告请求应在途";

    api.setToken(QStringLiteral("token-B"));   // 登出/换号

    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "过期响应后未配平";
    pumpEvents(100);
    EXPECT_TRUE(vm.announcements().isEmpty()) << "旧账号公告不得落入新界面";
    EXPECT_EQ(seenSignals, 0) << "过期响应不得发 seen";
    EXPECT_EQ(stub.readStateReqCount.load(), 0);
}

// ==================== A5：clearAll 归零并重置已上报水位 ====================
TEST_F(AnnouncementVmTest, clearAll归零并允许重新上报)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return vm.announcements().size() == 20 && !vm.isLoading(); }));
    ASSERT_TRUE(waitUntil([&]() { return stub.readStateReqCount.load() == 1; })) << "水位上报未到达";

    vm.clearAll();
    EXPECT_TRUE(vm.announcements().isEmpty());
    EXPECT_FALSE(vm.hasMore());
    EXPECT_FALSE(vm.isLoading());
    EXPECT_FALSE(vm.isRefreshing());

    // 水位记账已清零：换号重进可重新上报同一公告集
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() {
        return stub.readStateReqCount.load() == 2 && !vm.isLoading();
    })) << "clearAll 后应允许重新上报";
}

// ==================== A6：预览拉取不触发已读语义 ====================
TEST_F(AnnouncementVmTest, 预览拉取不触发已读语义)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    int seenSignals = 0;
    int errorSignals = 0;
    QObject::connect(&vm, &AnnouncementViewModel::announcementsSeen, [&]() { seenSignals++; });
    QObject::connect(&vm, &AnnouncementViewModel::errorOccurred, [&](const QString&) { errorSignals++; });

    vm.loadPreview();
    ASSERT_TRUE(waitUntil([&]() { return vm.previewTitle() == QStringLiteral("公告922"); })) << "预览未落位";
    EXPECT_EQ(vm.previewCreatedAt(), QStringLiteral("2026-09-01T00:00:00Z"));

    pumpEvents(150);
    EXPECT_EQ(seenSignals, 0) << "预览不得发 announcementsSeen（红点不能被预览误清）";
    EXPECT_EQ(stub.readStateReqCount.load(), 0) << "预览不得上报水位";
    EXPECT_TRUE(vm.announcements().isEmpty()) << "预览不得改动列表";
    EXPECT_FALSE(vm.isLoading()) << "预览不驱动加载指示器";
    EXPECT_EQ(errorSignals, 0);
}

// ==================== A7：预览失败静默保留旧值 ====================
TEST_F(AnnouncementVmTest, 预览失败静默保留旧值)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    vm.loadPreview();
    ASSERT_TRUE(waitUntil([&]() { return vm.previewTitle() == QStringLiteral("公告922"); }));

    stub.listFail = true;
    vm.loadPreview();
    ASSERT_TRUE(waitUntil([&]() { return stub.listReqCount.load() == 2; }));
    pumpEvents(100);
    EXPECT_EQ(vm.previewTitle(), QStringLiteral("公告922")) << "失败必须保留旧值";
}

// ==================== A8：clearAll 同步清空预览 ====================
TEST_F(AnnouncementVmTest, clearAll清空预览值)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    vm.loadPreview();
    ASSERT_TRUE(waitUntil([&]() { return vm.previewTitle() == QStringLiteral("公告922"); }));

    vm.clearAll();
    EXPECT_TRUE(vm.previewTitle().isEmpty());
    EXPECT_TRUE(vm.previewCreatedAt().isEmpty());
}

// ==================== A9：隐藏公告后刷新不复活、clearAll 恢复 ====================
TEST_F(AnnouncementVmTest, 隐藏公告后刷新不复活且清空恢复)
{
    AnnouncementStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    AnnouncementViewModel vm(&api);

    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return vm.announcements().size() == 20 && !vm.isLoading(); }));

    // 隐藏第一页首条（id 922）
    vm.hideAnnouncement(922);
    EXPECT_EQ(vm.announcements().size(), 19);

    // 同会话刷新：隐藏项不得复活（第一页 20 条过滤掉隐藏的 1 条）
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading() && vm.announcements().size() == 19; }));
    pumpEvents(100);
    EXPECT_EQ(vm.announcements().size(), 19) << "同会话刷新不得复活隐藏项";
    for (const auto& v : vm.announcements())
        EXPECT_NE(v.toMap()["id"].toInt(), 922);

    // 续页也不复活：第一页 19 + 第二页 2 = 21
    vm.loadNextPage();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading() && vm.announcements().size() == 21; }));

    // clearAll 清空隐藏集合 → 刷新后恢复
    vm.clearAll();
    vm.refresh();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading() && vm.announcements().size() == 20; }));
    EXPECT_EQ(vm.announcements().size(), 20) << "clearAll 后隐藏集合应清空（922 恢复）";
}

} // namespace
