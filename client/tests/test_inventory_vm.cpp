// InventoryViewModel 会话切换防护测试
//
// 覆盖 fd39eb62 审查发现的问题 #4（跨账号在途响应残留）：
//   V1  正常加载：响应填充 items、isLoading 复位
//   V2  会话切换（登出/换号）后到达的过期响应：不填充旧账号数据、加载标记不卡死
//   V3  clearAll：清空数据与加载状态（Main.qml 登出分支调用）
//   V4  未登录守卫拦截（Silent "请先登录"）：不发请求、items 保持空、isLoading 复位
//
// 离线策略（在线优先 + 联网即同步）用例：
//   断网无快照→loadFailed；断网有快照→静默兜底；成功→快照落库；
//   onNetworkRestored 重拉（健康数据不打扰）；退避重试自动恢复；clearAll 停表复位；
//   服务器繁忙 503→退避自动恢复；过滤态快照兜底按词过滤/重派生；
//   恢复边沿在自身在途响应到达时不重复拉取；main.cpp 接线由旁路请求触发自动重拉；
//   删除成功→快照行同步移除/删除失败回滚快照不变；非瞬时失败有数据→页内提示；
//   瞬时失败有数据→静默保留；重试 tick 在途守卫；
//   viewState 派生（QML 三处组合判断的收敛）：四态 Empty/InitialLoading/Offline/Content、
//   离线态优先于初始加载（重试在途不闪烁）、快照兜底→Content、依赖信号转发（QML 绑定刷新）、
//   枚举经 QML 类型系统按「类型.值」可解析（InventoryPage 绑定契约）

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSqlDatabase>
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
#include "LocalDatabase.h"
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
    std::atomic<int> busy503Times{0};   // >0 时接下来的请求返回 503 并递减（服务器瞬时繁忙模拟）
    std::atomic<int> fail500Times{0};   // >0 时接下来的 GET 返回 500 并递减（非瞬时失败：页内提示用例）
    std::atomic<int> deleteReqCount{0}; // DELETE 请求计数
    std::atomic<int> deleteFailTimes{0}; // >0 时接下来的 DELETE 返回 500 并递减（删除失败回滚用例）

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
            // 服务器瞬时繁忙模拟（503）：按次数返回，之后恢复正常（“503 自动退避恢复”用例）
            if (busy503Times.load() > 0) {
                busy503Times--;
                res.status = 503;
                res.set_content(R"({"error":"系统繁忙，请稍后重试"})", "application/json");
                respondedCount++;
                return;
            }
            // 非瞬时服务器错误模拟（500）：按次数返回，之后恢复正常（“有数据非瞬时失败页内提示”用例）
            if (fail500Times.load() > 0) {
                fail500Times--;
                res.status = 500;
                res.set_content(R"({"error":"服务器内部错误，请稍后重试"})", "application/json");
                respondedCount++;
                return;
            }
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

        // 删除库存：默认 200；deleteFailTimes>0 时按次数返回 500（删除失败回滚用例）
        svr.Delete("/api/inventory/:id", [this](const httplib::Request&, httplib::Response& res) {
            deleteReqCount++;
            if (deleteFailTimes.load() > 0) {
                deleteFailTimes--;
                res.status = 500;
                res.set_content(R"({"error":"服务器内部错误，请稍后重试"})", "application/json");
                return;
            }
            res.status = 200;
            res.set_content(R"({"message":"已删除"})", "application/json");
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
        // 独立内存库注入 VM（不触生产数据路径）；预置用户行使快照写路径可用
        connName = QStringLiteral("gocook_invtest_%1").arg(s_counter++);
        testDb = LocalDatabase::createForTesting(QStringLiteral(":memory:"), connName);
        ASSERT_TRUE(testDb != nullptr);
        ASSERT_TRUE(testDb->isOpen());
        ASSERT_TRUE(testDb->saveUser(1, QStringLiteral("tester"), QStringLiteral("token-A")));
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

int InventoryVmTest::s_counter = 0;

// ==================== V1：正常加载 ====================
TEST_F(InventoryVmTest, 正常加载填充数据)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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

    InventoryViewModel vm(&api, nullptr, testDb);
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
// ==================== V10+：离线策略（快照兜底 / 联网即同步 / 退避重试） ====================

TEST_F(InventoryVmTest, 断网无快照置loadFailed且不发页内提示)
{
    // 指向无监听端口：连接拒绝 → 网络层错误
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "失败回调超时";
    EXPECT_TRUE(vm.loadFailed()) << "无快照可兜 → 置 loadFailed（离线视图呈现）";
    EXPECT_EQ(vm.loadFailedMessage(), HttpGoCookApi::kNetworkErrorMessage)
        << "离线视图文案必须按失败类型透传（网络层错误）";
    EXPECT_TRUE(vm.items().isEmpty());
    EXPECT_FALSE(errorFlag.load()) << "首页失败不走 errorOccurred（一个失败一个反馈，呈现交给离线视图）";
    EXPECT_FALSE(testDb->hasInventoryCache()) << "失败不得凭空生成快照";

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

TEST_F(InventoryVmTest, 断网有快照静默兜底不发提示)
{
    // 预置快照（模拟上次在线成功同步）
    QVariantList snapshot;
    snapshot << QVariantMap{{"id", 7}, {"ingredientName", "番茄"}, {"quantity", 3.0},
                            {"unit", "个"}, {"addedAt", "2026-05-01T00:00:00Z"}};
    snapshot << QVariantMap{{"id", 8}, {"ingredientName", "盐"}, {"quantity", 5.0},
                            {"unit", "克"}, {"addedAt", "2026-05-02T00:00:00Z"}};
    ASSERT_TRUE(testDb->saveInventoryCache(snapshot));
    ASSERT_TRUE(testDb->hasInventoryCache());

    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 2; })) << "快照兜底未落地";
    EXPECT_FALSE(vm.loadFailed()) << "有快照不得进入离线态";
    EXPECT_FALSE(errorFlag.load()) << "快照兜底必须静默（零提示，与联网状态无异）";
    EXPECT_EQ(vm.items()[0].toMap()["ingredientName"].toString(), "番茄");
    EXPECT_EQ(vm.items()[1].toMap()["ingredientName"].toString(), "盐");
    EXPECT_FALSE(vm.isLoading());

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

TEST_F(InventoryVmTest, 成功加载刷新本地快照)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "加载超时";

    EXPECT_TRUE(testDb->hasInventoryCache()) << "成功加载必须落快照（在线优先）";
    const QVariantList cached = testDb->getInventoryCache();
    ASSERT_EQ(cached.size(), 1);
    EXPECT_EQ(cached[0].toMap()["ingredientName"].toString(), "番茄");
    EXPECT_EQ(cached[0].toMap()["unit"].toString(), "个");

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

TEST_F(InventoryVmTest, 网络恢复自动重拉)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 先断网
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";

    // 恢复联网（切回桩服务）→ 网络恢复触发自动重拉
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    vm.onNetworkRestored();

    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; })) << "自动重拉未落地";
    EXPECT_EQ(vm.items()[0].toMap()["ingredientName"].toString(), "番茄");
    EXPECT_FALSE(vm.loadFailed()) << "重拉成功后必须退出离线态";
    EXPECT_TRUE(vm.loadFailedMessage().isEmpty()) << "退出离线态应清空文案";
    EXPECT_FALSE(vm.isLoading());
}

TEST_F(InventoryVmTest, 网络恢复时健康数据不重拉)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading() && vm.items().size() == 1; })) << "加载超时";
    const int reqCount = stub.inventoryReqCount.load();

    vm.onNetworkRestored(); // 数据健康（加载成功、有数据）→ 不打扰，避免列表无谓跳动
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(stub.inventoryReqCount.load(), reqCount) << "健康数据不得触发重拉";
}

TEST_F(InventoryVmTest, 退避重试自动恢复)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 先断网
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.setRetryDelaysMs({50, 50, 50}); // 注入短退避，测试用
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";

    // 恢复联网：不再手动触发，等退避定时器自动重拉
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; }, 3000)) << "退避重试未自动恢复";
    EXPECT_FALSE(vm.loadFailed()) << "重试成功后必须退出离线态";
}

TEST_F(InventoryVmTest, clearAll复位失败态并停重试)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.setRetryDelaysMs({30, 30});
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";

    vm.clearAll();
    EXPECT_FALSE(vm.loadFailed());
    EXPECT_TRUE(vm.loadFailedMessage().isEmpty()) << "clearAll 应清理离线态文案";
    EXPECT_TRUE(vm.items().isEmpty());

    // 若重试未被停表，下一档（30ms）会打到桩服务；桩服务一个请求都不应收到
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(stub.inventoryReqCount.load(), 0) << "clearAll 必须停掉退避重试";
}

TEST_F(InventoryVmTest, 服务器繁忙503自动退避恢复)
{
    InventoryStubServer stub;
    stub.busy503Times = 1; // 首个请求 503（连接池饱和语义），之后恢复
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.setRetryDelaysMs({50, 50, 50});
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; }, 3000)) << "503 后未自动重试恢复";
    EXPECT_FALSE(vm.loadFailed()) << "重试成功后必须退出离线态";
    EXPECT_FALSE(errorFlag.load()) << "503（瞬时故障）不走页内提示";
    EXPECT_EQ(stub.inventoryReqCount.load(), 2) << "首个 503 + 重试成功恰好两次请求";

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

TEST_F(InventoryVmTest, 过滤态快照兜底按词过滤且可重派生)
{
    // 预置快照（两条：番茄、盐）——断网时唯一数据源
    QVariantList snapshot;
    snapshot << QVariantMap{{"id", 7}, {"ingredientName", "番茄"}, {"quantity", 3.0}, {"unit", "个"}};
    snapshot << QVariantMap{{"id", 8}, {"ingredientName", "盐"}, {"quantity", 5.0}, {"unit", "克"}};
    ASSERT_TRUE(testDb->saveInventoryCache(snapshot));

    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 断网：只有快照可用
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    // 1) 首页 + 过滤词“番茄”失败 → 快照按词过滤（只显示番茄，与过滤框一致）
    vm.setFilterText(QStringLiteral("番茄"));
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; })) << "过滤态快照兜底未落地";
    EXPECT_EQ(vm.items()[0].toMap()["ingredientName"].toString(), QStringLiteral("番茄"));
    EXPECT_FALSE(vm.loadFailed());

    // 2) 换过滤词“盐”再失败 → 从全量快照重派生（不回滚旧词子集、也不在子集上二次过滤）
    vm.setFilterText(QStringLiteral("盐"));
    ASSERT_TRUE(waitUntil([&]() {
        return vm.items().size() == 1
            && vm.items()[0].toMap()["ingredientName"].toString() == QStringLiteral("盐");
    })) << "换词后未从快照重派生";
    EXPECT_FALSE(vm.loadFailed());

    // 3) 清空过滤词再失败 → 恢复全量快照
    vm.setFilterText(QString());
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 2; })) << "清词后未恢复全量";
    EXPECT_FALSE(vm.loadFailed());
    EXPECT_FALSE(errorFlag.load()) << "快照兜底必须静默（零提示）";

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

TEST_F(InventoryVmTest, 恢复边沿在自身在途响应到达时不重复拉取)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    // 与 main.cpp 相同接线：网络恢复信号直达 VM
    QObject::connect(&api, &HttpGoCookApi::networkRestored, &vm, &InventoryViewModel::onNetworkRestored);

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";

    // 恢复联网后由 VM 自身重拉：该成功响应到达时先触发恢复边沿（emit 在回调链前），
    // 此刻 m_isLoading 仍为 true → 守卫拦截，不得二次拉取、原响应不得被 epoch 顶掉
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; })) << "重拉未落地";
    EXPECT_FALSE(vm.loadFailed());
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(stub.inventoryReqCount.load(), 1) << "在途守卫失效：重复拉取或响应被顶掉";
    EXPECT_FALSE(vm.isLoading());
}

TEST_F(InventoryVmTest, 接线契约_网络恢复由旁路请求触发自动重拉)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 先失联
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    // 与 main.cpp 相同接线：网络恢复信号直达 VM 槽
    QObject::connect(&api, &HttpGoCookApi::networkRestored, &vm, &InventoryViewModel::onNetworkRestored);

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";
    EXPECT_TRUE(vm.items().isEmpty());

    // 恢复联网：由一个旁路请求（非 VM 发起）的首个成功响应触发恢复边沿
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    std::atomic<bool> sideDone{false};
    api.getInventory(1, 50, "", [&](bool, const gocook::models::PagedInventory&, const std::string&) {
        sideDone = true;
    });
    ASSERT_TRUE(waitUntil(sideDone)) << "旁路请求超时";

    // 无需手调 onNetworkRestored：接线应已驱动 VM 自动重拉
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; })) << "接线未触发自动重拉";
    EXPECT_FALSE(vm.loadFailed());
    EXPECT_EQ(stub.inventoryReqCount.load(), 2) << "旁路 1 次 + VM 自动重拉 1 次";
}

// ==================== 删除成功：快照行同步移除（防离线兜底复活已删项） ====================
TEST_F(InventoryVmTest, 删除成功同步移除快照行防离线复活)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "加载超时";
    ASSERT_TRUE(testDb->hasInventoryCache());
    ASSERT_EQ(testDb->getInventoryCache().size(), 1);

    itemsChangedFlag = false;
    vm.deleteItem(7); // 乐观移除 → API 删除 → 成功回调同步快照行
    ASSERT_TRUE(waitUntil([&]() { return stub.deleteReqCount.load() >= 1; })) << "删除请求未发出";
    ASSERT_TRUE(waitUntil([&]() { return testDb->getInventoryCache().isEmpty(); }))
        << "删除成功后快照必须同步移除该行（防断网兜底复活已删项）";
    EXPECT_TRUE(testDb->hasInventoryCache()) << "meta 保留：空快照（真实空库存）≠ 从未同步";
    EXPECT_TRUE(vm.items().isEmpty());

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
}

// ==================== 删除失败：列表回滚 + 快照不被误改 ====================
TEST_F(InventoryVmTest, 删除失败快照保持不变且列表回滚)
{
    InventoryStubServer stub;
    stub.deleteFailTimes = 1; // 首次删除 500（非瞬时，仅回滚不重试）
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> errorFlag{false};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&]() { errorFlag = true; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return testDb->hasInventoryCache(); })) << "加载超时";
    ASSERT_EQ(vm.items().size(), 1);

    vm.deleteItem(7);
    ASSERT_TRUE(waitUntil(errorFlag)) << "删除失败回调超时";

    // 回滚：列表恢复；快照未动（失败不得误删快照行）
    EXPECT_EQ(vm.items().size(), 1);
    EXPECT_EQ(vm.items()[0].toMap()["id"].toInt(), 7);
    ASSERT_TRUE(testDb->hasInventoryCache());
    const QVariantList kept = testDb->getInventoryCache();
    ASSERT_EQ(kept.size(), 1);
    EXPECT_EQ(kept[0].toMap()["ingredientName"].toString(), QStringLiteral("番茄"));

    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

// ==================== 非瞬时首页失败（有数据）：页内提示且数据保留 ====================
TEST_F(InventoryVmTest, 非瞬时首页失败有数据时页内提示且数据保留)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });
    std::atomic<int> errorCount{0};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&](const QString&) { errorCount++; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "首次加载超时";
    ASSERT_EQ(vm.items().size(), 1);

    // 下一次首页加载失败（500 非瞬时、非快照）：数据保留 + 恰一次页内提示
    stub.fail500Times = 1;
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "失败回调超时";
    ASSERT_TRUE(waitUntil([&]() { return errorCount.load() >= 1; })) << "非瞬时失败必须页内提示";
    EXPECT_EQ(errorCount.load(), 1) << "恰好一条反馈（一个失败一个反馈）";
    EXPECT_EQ(vm.items().size(), 1) << "失败不得清空可显示数据";
    EXPECT_FALSE(vm.loadFailed()) << "有数据可显示不得进离线视图";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

// ==================== 瞬时首页失败（有数据）：静默保留 + 退避重试 ====================
TEST_F(InventoryVmTest, 瞬时首页失败有数据时静默保留)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    // 重试档位远离观测窗口，避免自动重拉干扰断言
    vm.setRetryDelaysMs({5000, 5000});
    std::atomic<bool> itemsChangedFlag{false};
    QObject::connect(&vm, &InventoryViewModel::itemsChanged, [&]() { itemsChangedFlag = true; });
    std::atomic<int> errorCount{0};
    QObject::connect(&vm, &InventoryViewModel::errorOccurred, [&](const QString&) { errorCount++; });

    vm.loadInventory();
    ASSERT_TRUE(waitUntil(itemsChangedFlag)) << "首次加载超时";
    ASSERT_EQ(vm.items().size(), 1);

    // 换到无监听端口（网络层错误，瞬时）：数据保留 + 静默（不发页内提示）
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "失败回调超时";
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(errorCount.load(), 0) << "瞬时故障走退避重试，不打扰（静默）";
    EXPECT_EQ(vm.items().size(), 1) << "网络错误保留旧数据";

    QObject::disconnect(&vm, &InventoryViewModel::itemsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &InventoryViewModel::errorOccurred, nullptr, nullptr);
}

// ==================== 重试 tick 在途守卫：不插请求、不断链 ====================
TEST_F(InventoryVmTest, 重试tick不打断在途请求)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 先断网
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.setRetryDelaysMs({80, 80, 80});
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";

    // 恢复联网但保持慢响应：手动重拉在途期间让退避 tick 命中
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    stub.delayMs = 400;
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return stub.inventoryReqCount.load() >= 1; })) << "手动重拉未发出";

    // 等 tick（80ms）多次命中在途窗口：不得插新请求（否则在途响应会被 epoch 顶掉）
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_EQ(stub.inventoryReqCount.load(), 1) << "在途期间 tick 不得插请求";

    // 慢响应（400ms）落地：成功 → 清失败态、停表
    ASSERT_TRUE(waitUntil([&]() { return stub.respondedCount.load() >= 1; })) << "慢响应未完成";
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "响应落地后加载标记未复位";
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_FALSE(vm.loadFailed()) << "在途请求成功后应退出失败态";
    EXPECT_EQ(stub.inventoryReqCount.load(), 1) << "整段过程恰一次请求（无 tick 插队）";
}

// ==================== viewState 派生：四态与依赖信号转发 ====================
TEST_F(InventoryVmTest, viewState派生四态与信号转发)
{
    InventoryStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    std::atomic<int> viewStateChangedCount{0};
    QObject::connect(&vm, &InventoryViewModel::viewStateChanged, [&]() { viewStateChangedCount++; });

    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Empty)
        << "初始：无数据、未加载、未失败 → Empty";

    stub.delayMs = 300; // 慢响应：制造「在途且无数据」窗口，断言 InitialLoading
    vm.loadInventory();
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::InitialLoading)
        << "在途且无数据 → InitialLoading";
    EXPECT_GE(viewStateChangedCount.load(), 1) << "isLoading 变化必须转发 viewStateChanged";

    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "加载超时";
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Content) << "有数据 → Content";
    EXPECT_GE(viewStateChangedCount.load(), 3)
        << "items/isLoading 变化须持续转发（QML 绑定刷新）";

    QObject::disconnect(&vm, &InventoryViewModel::viewStateChanged, nullptr, nullptr);
}

TEST_F(InventoryVmTest, viewState派生离线态优先于初始加载)
{
    InventoryStubServer stub;
    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1")); // 先断网（无快照）
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.loadFailed(); })) << "断网失败态超时";
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Offline) << "失败且无数据 → Offline";

    // 切到慢响应服务手动重拉：在途期间 loadFailed 仍为真、无数据 → 派生必须保持 Offline
    // （判定顺序 Offline 优先于 InitialLoading——原组合表达式不含 !isLoading，离线视图不闪烁）
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    stub.delayMs = 300;
    vm.loadInventory();
    ASSERT_TRUE(vm.isLoading()) << "重拉必须在途";
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Offline)
        << "重试在途不得回退为 InitialLoading";

    // 响应落地成功：失败态清除 → Content
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "重拉超时";
    EXPECT_FALSE(vm.loadFailed());
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Content);
}

TEST_F(InventoryVmTest, viewState派生快照兜底为Content)
{
    // 预置快照（上次在线同步）：断网 → 静默兜底，有数据可显示 → Content（不进离线态）
    QVariantList snapshot;
    snapshot << QVariantMap{{"id", 7}, {"ingredientName", "番茄"}, {"quantity", 3.0},
                            {"unit", "个"}, {"addedAt", "2026-05-01T00:00:00Z"}};
    ASSERT_TRUE(testDb->saveInventoryCache(snapshot));

    api.setBaseUrl(QStringLiteral("http://127.0.0.1:1"));
    api.setToken(QStringLiteral("token-A"));

    InventoryViewModel vm(&api, nullptr, testDb);
    vm.loadInventory();
    ASSERT_TRUE(waitUntil([&]() { return vm.items().size() == 1; })) << "快照兜底未落地";
    EXPECT_FALSE(vm.loadFailed()) << "快照兜底不进离线态";
    EXPECT_EQ(vm.viewState(), InventoryViewModel::ViewState::Content) << "快照兜底 → 有数据可显示";
}

// ==================== viewState 枚举的 QML 可见性（InventoryPage 绑定契约） ====================
// InventoryPage.qml 以 InventoryViewModel.Empty / InitialLoading / Offline 收敛三处判定，
// 依赖 Q_ENUM + qmlRegisterUncreatableType（main.cpp 中的注册形态与此一致）。本用例验证枚举值经
// QML 类型系统按「类型.值」解析且数值与 C++ 一致——防 Q_ENUM 遗漏/枚举重排/类型未注册导致页面绑定失配。
TEST_F(InventoryVmTest, viewState枚举可被QML解析且与Cpp取值一致)
{
    qmlRegisterUncreatableType<InventoryViewModel>("client", 1, 0, "InventoryViewModel",
                                                   "仅用于枚举访问（视图状态），不可实例化");

    QQmlEngine engine;
    QQmlComponent comp(&engine);
    comp.setData(
        "import QtQml\n"
        "import client\n"
        "QtObject {\n"
        "    property var initialLoading: InventoryViewModel.InitialLoading\n"
        "    property var empty: InventoryViewModel.Empty\n"
        "    property var offline: InventoryViewModel.Offline\n"
        "    property var content: InventoryViewModel.Content\n"
        "}",
        QUrl());
    QScopedPointer<QObject> root(comp.create());
    ASSERT_FALSE(comp.isError()) << qPrintable(comp.errorString());
    ASSERT_TRUE(root != nullptr);

    EXPECT_EQ(root->property("initialLoading").toInt(),
              static_cast<int>(InventoryViewModel::ViewState::InitialLoading));
    EXPECT_EQ(root->property("empty").toInt(),
              static_cast<int>(InventoryViewModel::ViewState::Empty));
    EXPECT_EQ(root->property("offline").toInt(),
              static_cast<int>(InventoryViewModel::ViewState::Offline));
    EXPECT_EQ(root->property("content").toInt(),
              static_cast<int>(InventoryViewModel::ViewState::Content));
}

} // namespace
