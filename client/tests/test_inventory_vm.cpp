// InventoryViewModel 会话切换防护测试
//
// 覆盖 fd39eb62 审查发现的问题 #4（跨账号在途响应残留）：
//   V1  正常加载：响应填充 items、isLoading 复位
//   V2  会话切换（登出/换号）后到达的过期响应：不填充旧账号数据、加载标记不卡死
//   V3  clearAll：清空数据与加载状态（Main.qml 登出分支调用）
//   V4  未登录守卫拦截（Silent "请先登录"）：不发请求、items 保持空、isLoading 复位

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QString>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include "HttpGoCookApi.h"
#include "InventoryViewModel.h"
#include <httplib/httplib.h>

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
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return true;
}

// 库存桩服务：GET /api/inventory 带可选延迟（模拟慢网络下的在途请求）
class InventoryStubServer {
public:
    std::atomic<int> inventoryReqCount{0};
    std::atomic<int> delayMs{0};

    InventoryStubServer()
    {
        svr.Get("/api/inventory", [this](const httplib::Request&, httplib::Response& res) {
            inventoryReqCount++;
            const int delay = delayMs.load();
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            res.status = 200;
            res.set_content(
                R"({"pagination":{"page":1,"size":50,"total":1,"total_pages":1},)"
                R"("data":[{"id":7,"ingredient_name":"番茄","quantity":3.0,"unit":"个",)"
                R"("expiry_date":"2026-05-10","added_at":"2026-05-01T00:00:00Z"}]})",
                "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("InventoryStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });
    }

    ~InventoryStubServer()
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

class InventoryVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QString()); // 每个用例从未登录开始（避免 token 污染）
    }

    HttpGoCookApi api;
};

// ==================== V1：正常加载 ====================
TEST_F(InventoryVmTest, 正常加载填充数据)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "加载超时";
    EXPECT_EQ(vm.items().size(), 1);
    EXPECT_EQ(vm.items()[0].toMap()["ingredientName"].toString(), QStringLiteral("番茄"));
    EXPECT_FALSE(vm.isLoading());

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V2：会话切换后过期响应被丢弃 ====================
TEST_F(InventoryVmTest, 会话切换后过期响应被丢弃)
{
    InventoryStubServer stub;
    stub.delayMs = 300; // 慢响应：请求在途期间完成登出
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.loadInventory();      // token-A 的请求在途
    api.setToken(QString()); // 登出（token 变化 → 快照不匹配）
    vm.clearAll();           // Main.qml 登出分支调用（此时 itemsChanged 会被触发，属预期）
    itemsChangedFlag = false; // 清掉 clearAll 的信号，只观察过期响应

    std::this_thread::sleep_for(std::chrono::milliseconds(600)); // 等过期响应到达
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);  // 派发 finished 回调

    EXPECT_EQ(stub.inventoryReqCount.load(), 1) << "请求确实在登出前发出过";
    EXPECT_FALSE(itemsChangedFlag.load()) << "过期响应不得填充数据/触发 itemsChanged";
    EXPECT_TRUE(vm.items().isEmpty()) << "旧账号数据不得串入";
    EXPECT_FALSE(vm.isLoading()) << "加载标记必须复位";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V3：clearAll ====================
TEST_F(InventoryVmTest, clearAll清空数据与状态)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "加载超时";
    ASSERT_EQ(vm.items().size(), 1);

    vm.clearAll();
    EXPECT_TRUE(vm.items().isEmpty());
    EXPECT_FALSE(vm.isLoading());
    EXPECT_FALSE(vm.hasMore());

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V4：未登录守卫拦截 ====================
TEST_F(InventoryVmTest, 未登录守卫拦截不发请求)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl())); // 无 token

    InventoryViewModel vm(&api);
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(errorFlag)) << "守卫回调超时";
    EXPECT_EQ(stub.inventoryReqCount.load(), 0) << "未登录不得发出请求";
    EXPECT_TRUE(vm.items().isEmpty());
    EXPECT_FALSE(vm.isLoading());

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}
} // namespace
