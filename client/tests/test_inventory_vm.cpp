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
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "HttpGoCookApi.h"
#include "InventoryViewModel.h"
#include <httplib/httplib.h>

namespace {

// 轮询事件循环直到条件满足或超时（QNetworkAccessManager 是异步的，需要事件循环推进）。
// 双形态：std::atomic<bool>（既有用例）与任意可调用谓词（lambda）共用
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

// 库存桩服务：GET /api/inventory 带可选延迟（模拟慢网络下的在途请求）；
// 分页可配置：totalPages>1 时首页返 1 行番茄、后续页返 1 行土豆（VM 翻页/过滤翻页断言用）
class InventoryStubServer {
public:
    std::atomic<int> inventoryReqCount{0};
    std::atomic<int> delayMs{0};
    std::atomic<int> totalPages{1};
    std::atomic<int> respondedCount{0}; // 已写响应的请求数（sleep 之后递增：慢响应“服务端已完成”标记）

    // 最近一次请求携带的 keyword（库存页过滤框断言用）
    std::string lastKeyword() const
    {
        std::lock_guard<std::mutex> lock(mu);
        return seenKeyword;
    }

    // 最近一次请求携带的 page（过滤态翻页断言用）
    int lastPage() const
    {
        std::lock_guard<std::mutex> lock(mu);
        return seenPage;
    }

    InventoryStubServer()
    {
        svr.Get("/api/inventory", [this](const httplib::Request& req, httplib::Response& res) {
            inventoryReqCount++;
            const int page = req.has_param("page") ? std::atoi(req.get_param_value("page").c_str()) : 1;
            {
                std::lock_guard<std::mutex> lock(mu);
                seenKeyword = req.get_param_value("keyword");
                seenPage = page;
            }
            const int delay = delayMs.load();
            if (delay > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            const int tp = totalPages.load();
            res.status = 200;
            if (page <= 1) {
                // 首页：固定 1 行番茄数据（V1 断言依赖），total_pages 由 totalPages 配置
                res.set_content(
                    R"({"pagination":{"page":1,"size":50,"total":)" +
                        std::to_string(tp) +
                        R"(,"total_pages":)" + std::to_string(tp) +
                        R"(},"data":[{"id":7,"ingredient_name":"番茄","quantity":3.0,"unit":"个",)"
                        R"("expiry_date":"2026-05-10","added_at":"2026-05-01T00:00:00Z"}]})",
                    "application/json");
            } else {
                // 后续页：1 行“土豆”数据（id 8，区分于首页番茄）——供翻页追加语义断言
                res.set_content(
                    R"({"pagination":{"page":)" + std::to_string(page) +
                        R"(,"size":50,"total":)" + std::to_string(tp) +
                        R"(,"total_pages":)" + std::to_string(tp) +
                        R"(},"data":[{"id":8,"ingredient_name":"土豆","quantity":2.0,"unit":"个",)"
                        R"("expiry_date":"2026-05-20","added_at":"2026-05-02T00:00:00Z"}]})",
                    "application/json");
            }
            respondedCount++;
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("InventoryStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running()
        // 显式暴露启动失败；同时消除“端口已绑定但尚未开始监听”的竞态窗口
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("InventoryStubServer: failed to start");
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
    mutable std::mutex mu;         ///< 保护 seenKeyword
    std::string seenKeyword;       ///< 最近一次请求的 keyword 参数
    int seenPage = 1;              ///< 最近一次请求的 page 参数
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

// ==================== V5：过滤词随请求携带（库存页过滤框） ====================
TEST_F(InventoryVmTest, 设置过滤词请求携带keyword并回填)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.setFilterText(QStringLiteral("  料酒  "));
    EXPECT_EQ(vm.filterText(), QStringLiteral("料酒")) << "过滤词应 trim 后生效";
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "带过滤词的加载超时";

    EXPECT_EQ(stub.lastKeyword(), "料酒") << "请求必须携带 keyword=料酒";
    EXPECT_FALSE(vm.isLoading());

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V6：清空过滤词恢复全量请求 ====================
TEST_F(InventoryVmTest, 清空过滤词后请求不再携带keyword)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<int> changeCount{0};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { changeCount++; });

    vm.setFilterText(QStringLiteral("料酒"));
    ASSERT_TRUE(waitUntil([&]() { return changeCount.load() >= 1; })) << "过滤加载超时";
    EXPECT_EQ(stub.lastKeyword(), "料酒");

    vm.setFilterText(QString());
    ASSERT_TRUE(waitUntil([&]() { return changeCount.load() >= 2; })) << "清空后重载超时";
    EXPECT_EQ(vm.filterText(), QString());
    EXPECT_TRUE(stub.lastKeyword().empty()) << "清空过滤后请求不得携带 keyword";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V7：QML 调用路径回归（InventoryPage 过滤框） ====================
// 回归 6cf52ba：Q_PROPERTY 的 WRITE 只支持属性赋值，不会让 QML 生成可调用的 setFilterText ——
// 未声明 Q_INVOKABLE 时 InventoryPage.qml 报 "Property 'setFilterText' ... is not a function"，
// 过滤功能整体失效。本用例以 context property + QQmlEngine 最小场景复现 QML→C++ 方法调用形态
//（与库存页过滤框防抖回调同构），修复前必然失败：TypeError 使 setter 不执行、过滤词为空。
TEST_F(InventoryVmTest, QML可通过方法调用设置过滤词)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("inventoryVM", &vm);
    QQmlComponent comp(&engine);
    comp.setData(
        "import QtQml\n"
        "QtObject { Component.onCompleted: inventoryVM.setFilterText(\"  料酒  \") }",
        QUrl());
    QScopedPointer<QObject> root(comp.create());
    ASSERT_FALSE(comp.isError()) << qPrintable(comp.errorString());

    // 修复前：TypeError（setFilterText 不是函数）→ 调用未执行 → 过滤词为空，本断言失败
    EXPECT_EQ(vm.filterText(), QStringLiteral("料酒")) << "QML 方法调用 setFilterText 必须生效（trim 后）";
    EXPECT_TRUE(waitUntil(itemsChangedFlag)) << "带过滤词的加载超时";
    EXPECT_EQ(stub.lastKeyword(), "料酒") << "QML 触发加载必须携带 keyword";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V8：在途期间过滤词变化（请求代次语义回归） ====================
// 回归请求代次（m_epoch）语义：改词必须立即发出新请求（不得被在途请求押后），
// 旧词响应到达时代次已落后 → 静默丢弃：不落数据、不触发 itemsChanged、不补发冗余请求。
// 旧实现（单飞 + 快照词比对补发）下本用例两点必失败：新词请求在 R1 回归前发不出，
// 且 R1 回归会触发一次冗余补发（请求总数变为 3）。
TEST_F(InventoryVmTest, 在途期间过滤词变化丢弃过期响应并立即补发最新词)
{
    InventoryStubServer stub;
    stub.delayMs = 600; // R1 慢响应：制造“改词发生在 R1 在途期间”的窗口（为时序断言留裕量）
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<int> changeCount{0};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { changeCount++; });

    vm.setFilterText(QStringLiteral("料酒")); // R1（慢 600ms）在途
    ASSERT_TRUE(waitUntil([&]() { return stub.inventoryReqCount.load() >= 1; }))
        << "R1 必须已发出（此时在途）";
    EXPECT_EQ(stub.lastKeyword(), "料酒");

    QElapsedTimer timer;
    timer.start();
    stub.delayMs = 0; // 后续请求不再延迟：若新词请求被押后，必等到 R1 的 600ms 睡完才发出
    vm.setFilterText(QStringLiteral("料酒2")); // 改词：必须立即发出 R2（不被在途 R1 阻塞）
    ASSERT_TRUE(waitUntil([&]() { return stub.inventoryReqCount.load() >= 2; }))
        << "改词后新请求未发出（旧实现会押后到 R1 回归才补发）";
    EXPECT_LT(timer.elapsed(), 450) << "新词请求必须在 R1（600ms 延迟）回归之前发出";
    EXPECT_EQ(stub.lastKeyword(), "料酒2") << "R2 必须携带最新过滤词";

    // R2 已落地：最新词结果生效一次
    ASSERT_TRUE(waitUntil([&]() { return changeCount.load() >= 1; })) << "最新词结果未落地";
    EXPECT_EQ(vm.items().size(), 1);

    // 等 R1 在服务端完成（600ms 睡完、响应已写出）→ 客户端必然收到其回调；
    // 轮询“服务端完成”标记而非定睡：慢 CI 上 R1 回调晚到也不会造成空洞通过
    ASSERT_TRUE(waitUntil([&]() { return stub.respondedCount.load() >= 2; }))
        << "R1 迟迟未在服务端完成（慢响应被饿？）";
    // 给事件循环派发窗口，确认 R1 回调已处理
    waitUntil([&]() { return !vm.isLoading(); });
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(changeCount.load(), 1) << "过期响应不得落地/触发 itemsChanged";
    EXPECT_EQ(stub.inventoryReqCount.load(), 2) << "过期响应不得触发冗余补发（总请求数恰为 2）";
    EXPECT_FALSE(vm.isLoading()) << "加载标记须复位";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== V9：过滤态翻页自动携带 keyword ====================
TEST_F(InventoryVmTest, 过滤态翻页自动携带keyword)
{
    InventoryStubServer stub;
    stub.totalPages = 3;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api);
    std::atomic<int> changeCount{0};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { changeCount++; });

    vm.setFilterText(QStringLiteral("料酒"));
    ASSERT_TRUE(waitUntil([&]() { return changeCount.load() >= 1; })) << "过滤加载超时";
    EXPECT_EQ(stub.lastPage(), 1);
    EXPECT_TRUE(vm.hasMore()) << "total_pages=3，第一页后应还有下一页";

    vm.loadNextPage(); // 滚底翻页
    ASSERT_TRUE(waitUntil([&]() { return stub.lastPage() == 2; })) << "翻页请求未发出";
    EXPECT_EQ(stub.lastKeyword(), "料酒") << "翻页请求必须携带当前过滤词";
    ASSERT_TRUE(waitUntil([&]() { return changeCount.load() >= 2; })) << "翻页响应未落地";
    EXPECT_TRUE(vm.hasMore()) << "第 2/3 页后仍应可继续翻页";
    EXPECT_FALSE(vm.isLoading());
    // 追加而非替换：第 2 页数据（土豆）应拼接在第 1 页（番茄）之后
    ASSERT_EQ(vm.items().size(), 2) << "翻页必须追加新页数据而非替换已加载内容";
    EXPECT_EQ(vm.items()[0].toMap()["ingredientName"].toString(), QStringLiteral("番茄"));
    EXPECT_EQ(vm.items()[1].toMap()["ingredientName"].toString(), QStringLiteral("土豆"));

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}
} // namespace
