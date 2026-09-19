// ShoppingListViewModel 测试
//
// 覆盖会话切换防护（登出/换号后过期响应不得落入新界面）与登出统一清理：
//   S1  clearAll：清空列表/详情/乐观删除集合并各发信号；在途计数不被清（回调仍能 endLoad 配平）
//   S2  会话切换后列表响应被丢弃：数据不落 + 无信号 + 加载计数配平（isLoading 恢复正常）
//   S3  会话切换后详情响应被丢弃：currentList 不变 + 无 detailReady 信号 + 计数配平
//   S4  createListFromRecipe：v2.18 复合建单单请求——请求体携带 items、不再调批量端点、不再二次拉详情
//   S5  deleteShoppingListItem：单请求删除成功后本地移除条目（不发整表重拉）
//   S6  跨条目连点：a→b→c 三个 PATCH 按序送达、各自状态正确、乐观即时上屏（不丢更新）
//   S7  同条目连点：末态合并为一次补发；陈旧 ack 不回写（false 之后不得再出现 true）
//   S8  单条失败：错误上抛、该条目回滚到已确认态、队列继续（后续条目仍送达）
//   S9  clearAll：中断串行队列，在途返回后不再补发
//   S10 删除剔除：删除时剔除同条目待发勾选（不向已删行补发 PATCH）

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "HttpGoCookApi.h"
#include "ShoppingListViewModel.h"
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

// 详情条目断言辅助：按 id 查勾选态 / 存在性
bool itemChecked(const QVariantList& items, int id)
{
    for (const auto& v : items) {
        const QVariantMap item = v.toMap();
        if (item["id"].toInt() == id)
            return item["checked"].toBool();
    }
    return false;
}

bool itemPresent(const QVariantList& items, int id)
{
    for (const auto& v : items) {
        if (v.toMap()["id"].toInt() == id)
            return true;
    }
    return false;
}

// 购物清单桩服务：列表 / 详情 / 删除 / 创建（复合 items 回显）/ 批量加条目（delayMs 模拟慢网络）
class ShoppingListStubServer {
public:
    std::atomic<int> listReqCount{0};
    std::atomic<int> detailReqCount{0};
    std::atomic<int> deleteReqCount{0};
    std::atomic<int> createReqCount{0};    // 复合建单请求计数（v2.18）
    std::atomic<int> batchAddReqCount{0};  // 批量加点计数（复合建单上线后 createListFromRecipe 不应触碰）
    std::atomic<int> delayMs{0};        // 列表/详情响应延迟
    std::atomic<int> deleteDelayMs{0};  // 删除响应延迟（clearAll 用例让删除保持在途）
    std::atomic<int> deleteItemReqCount{0};  // 单条条目删除请求计数（v2.18 删除链路用例）

    std::atomic<int> updateItemReqCount{0};  // 条目勾选 PATCH 请求计数（连点合并用例）
    std::atomic<int> updateItemDelayMs{0};   // 勾选 PATCH 响应延迟（制造在途窗口）
    std::atomic<int> failUpdateItemId{0};    // 非 0：该 id 的 PATCH 返回 500（失败续跑用例）
    std::atomic<int> detailItemCount{1};     // 详情返回的条目数（id 自 501 起连续编号）

    std::mutex bodyMutex;               // createLastBody 跨线程读写保护（server 线程写 / 测试线程读）
    std::string createLastBody;

    std::mutex updateMx;                     // updateReqs 跨线程读写保护（server 线程写 / 测试线程读）
    std::vector<std::pair<int, bool>> updateReqs;   // 勾选 PATCH 流水：(itemId, checked) 按到达顺序

    ShoppingListStubServer()
    {
        svr.Get("/api/inventory/shopping-lists", [this](const httplib::Request&, httplib::Response& res) {
            listReqCount++;
            sleepIfNeeded(delayMs);
            res.status = 200;
            res.set_content(
                R"([{"id":11,"name":"清单A","item_count":2,"created_at":"2024-01-01T00:00:00Z"},)"
                R"({"id":12,"name":"清单B","item_count":0,"created_at":"2024-01-02T00:00:00Z"}])",
                "application/json");
        });

        svr.Get(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            detailReqCount++;
            sleepIfNeeded(delayMs);
            const std::string id = req.matches[1];
            // 条目数可配（detailItemCount）：id 自 501 起连续编号，默认 1 条保持既有用例形态
            std::string items = "[";
            for (int i = 0; i < detailItemCount.load(); ++i) {
                if (i > 0) items += ",";
                const std::string name = (i == 0) ? "番茄" : ("食材" + std::to_string(i + 1));
                items += R"({"id":)" + std::to_string(501 + i)
                       + R"(,"ingredient_name":")" + name
                       + R"(","required_quantity":4.0,"inventory_quantity":1.0,)"
                         R"("to_buy_quantity":3.0,"unit":"个","checked":false})";
            }
            items += "]";
            res.status = 200;
            res.set_content(R"({"id":)" + id + R"(,"name":"清单A","items":)" + items + "}", "application/json");
        });

        // 注：删除仅作计数/延迟用途——clearAll 在途计数配平用例
        svr.Delete(R"(/api/inventory/shopping-lists/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            deleteReqCount++;
            sleepIfNeeded(deleteDelayMs);
            res.status = 200;
            res.set_content(R"({"message":"已删除"})", "application/json");
        });

        // POST 创建（v2.18 复合建单：记录请求体，响应回显 items）
        svr.Post("/api/inventory/shopping-lists", [this](const httplib::Request& req, httplib::Response& res) {
            createReqCount++;
            {
                std::lock_guard<std::mutex> lk(bodyMutex);
                createLastBody = req.body;
            }
            sleepIfNeeded(delayMs);
            res.status = 201;
            res.set_content(
                R"({"id":21,"name":"番茄炒蛋","items":[)"
                R"({"id":601,"ingredient_name":"番茄","required_quantity":4.0,"inventory_quantity":0.0,)"
                R"("to_buy_quantity":4.0,"unit":"个","checked":false},)"
                R"({"id":602,"ingredient_name":"鸡蛋","required_quantity":2.0,"inventory_quantity":1.0,)"
                R"("to_buy_quantity":1.0,"unit":"个","checked":false}]})",
                "application/json");
        });

        // POST 批量加条目（不应被 createListFromRecipe 调用；注册以计数防回归）
        svr.Post(R"(/api/inventory/shopping-lists/(\d+)/items/batch)", [this](const httplib::Request&, httplib::Response& res) {
            batchAddReqCount++;
            res.status = 201;
            res.set_content(R"({"message":"已成功添加 1 项","items":[]})", "application/json");
        });

        // DELETE 单条条目（v2.18 删除链路；计数供 VM 用例断言）
        svr.Delete(R"(/api/inventory/shopping-lists/(\d+)/items/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            deleteItemReqCount++;
            res.status = 200;
            res.set_content(R"({"message":"清单项已删除"})", "application/json");
        });

        // PATCH 条目勾选（连点合并用例）：记录 (itemId, checked) 流水；支持延迟与指定 id 失败注入。
        // 请求由客户端串行发出，记录顺序即到达顺序
        svr.Patch(R"(/api/inventory/shopping-lists/(\d+)/items/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            updateItemReqCount++;
            const int itemId = std::stoi(req.matches[2]);
            const bool checked = QJsonDocument::fromJson(
                QByteArray::fromStdString(req.body)).object().value("checked").toBool();
            {
                std::lock_guard<std::mutex> lk(updateMx);
                updateReqs.emplace_back(itemId, checked);
            }
            sleepIfNeeded(updateItemDelayMs);
            if (failUpdateItemId.load() != 0 && failUpdateItemId.load() == itemId) {
                res.status = 500;
                res.set_content(R"({"error":"模拟服务端失败"})", "application/json");
                return;
            }
            res.status = 200;
            res.set_content(R"({"message":"清单项已更新"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("ShoppingListStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running()
        // 显式暴露启动失败；同时消除"端口已绑定但尚未开始监听"的竞态窗口
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("ShoppingListStubServer: failed to start");
    }

    ~ShoppingListStubServer()
    {
        svr.stop();
        if (th.joinable())
            th.join();
    }

    std::string baseUrl() const { return "http://127.0.0.1:" + std::to_string(port); }

    std::string createBodyCopy()
    {
        std::lock_guard<std::mutex> lk(bodyMutex);
        return createLastBody;
    }

    std::vector<std::pair<int, bool>> updateReqsCopy()
    {
        std::lock_guard<std::mutex> lk(updateMx);
        return updateReqs;
    }

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
};

class ShoppingListVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QStringLiteral("token-A"));
    }

    HttpGoCookApi api;
};

// ==================== S1：clearAll 清空数据与状态并各发信号 ====================
TEST_F(ShoppingListVmTest, clearAll清空数据与状态并各发信号)
{
    ShoppingListStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    // 先建立完整状态：列表（2 条） + 详情（id=11）
    std::atomic<bool> listLoaded{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListsChanged, [&]() {
        if (vm.shoppingLists().size() == 2)
            listLoaded = true;
    });
    vm.loadShoppingLists();
    ASSERT_TRUE(waitUntil(listLoaded)) << "列表加载超时";
    ASSERT_EQ(vm.shoppingLists().size(), 2);

    std::atomic<bool> detailLoaded{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailLoaded = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailLoaded)) << "详情加载超时";
    ASSERT_EQ(vm.currentList()["id"].toInt(), 11);

    // 制造 clearAll 期间的在途请求：
    //   a) 乐观删除保持在途（deleteDelayMs 300）——响应到达后不得复活已清理的列表数据
    //   b) 延迟的详情请求（delayMs 300）——验证在途计数不被 clearAll 清除（回调仍需 endLoad 配对）
    stub.deleteDelayMs = 300;
    stub.delayMs = 300;
    QVariantMap deletedList = vm.shoppingLists().first().toMap();
    vm.deleteShoppingListOptimistic(11, deletedList);
    EXPECT_EQ(vm.shoppingLists().size(), 1) << "乐观删除应立即从本地列表移除";
    vm.loadShoppingListDetail(11);
    EXPECT_TRUE(vm.isLoading()) << "详情请求在途：isLoading 应为真";

    // clearAll：数据与状态归零，三个状态信号各发一次
    int listsChanged = 0, currentChanged = 0, creatingChanged = 0;
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListsChanged, [&]() { listsChanged++; });
    QObject::connect(&vm, &ShoppingListViewModel::currentListChanged, [&]() { currentChanged++; });
    QObject::connect(&vm, &ShoppingListViewModel::creatingChanged, [&]() { creatingChanged++; });

    vm.clearAll();
    EXPECT_TRUE(vm.shoppingLists().isEmpty());
    EXPECT_TRUE(vm.currentList().isEmpty());
    EXPECT_FALSE(vm.creating());
    EXPECT_TRUE(vm.isLoading()) << "在途计数不被 clearAll 清除（回调仍需 endLoad 配对，防负漂移）";
    EXPECT_GE(listsChanged, 1) << "clearAll 必须发 shoppingListsChanged";
    EXPECT_GE(currentChanged, 1) << "clearAll 必须发 currentListChanged";
    EXPECT_GE(creatingChanged, 1) << "clearAll 必须发 creatingChanged";

    // 在途响应稍后到达：两个回调各自 endLoad 配平计数；乐观删除的响应不得复活已清理的列表
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "在途回调未配平计数";
    EXPECT_TRUE(vm.shoppingLists().isEmpty()) << "过期删除响应不得复活数据";
}

// ==================== S2：会话切换后列表过期响应被丢弃 ====================
TEST_F(ShoppingListVmTest, 会话切换后列表过期响应被丢弃)
{
    ShoppingListStubServer stub;
    stub.delayMs = 300; // 慢响应：请求在途期间完成登出
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    int listsChangedCount = 0;
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListsChanged, [&]() { listsChangedCount++; });

    vm.loadShoppingLists();
    ASSERT_TRUE(vm.isLoading()) << "列表请求应在途";

    api.setToken(QString()); // 登出（token 变化 → 快照不匹配）

    // 等过期响应到达并被丢弃：计数先行配平 → isLoading 恢复正常
    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "过期响应后计数未配平";
    EXPECT_EQ(stub.listReqCount.load(), 1) << "请求确实在登出前发出过";
    EXPECT_TRUE(vm.shoppingLists().isEmpty()) << "旧账号清单不得串入";
    EXPECT_EQ(listsChangedCount, 0) << "过期响应不得触发 shoppingListsChanged";
}

// ==================== S3：会话切换后详情过期响应被丢弃 ====================
TEST_F(ShoppingListVmTest, 会话切换后详情过期响应被丢弃)
{
    ShoppingListStubServer stub;
    stub.delayMs = 300; // 慢响应：请求在途期间完成登出
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });

    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(vm.isLoading()) << "详情请求应在途";

    api.setToken(QString()); // 登出

    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "过期响应后计数未配平";
    EXPECT_EQ(stub.detailReqCount.load(), 1) << "请求确实在登出前发出过";
    EXPECT_TRUE(vm.currentList().isEmpty()) << "旧账号详情不得写入";
    EXPECT_FALSE(detailReady.load()) << "过期响应不得发 shoppingListDetailReady";
}

// ==================== S4：createListFromRecipe 复合单请求（v2.18） ====================
TEST_F(ShoppingListVmTest, 由菜谱建清单单请求含条目且不再调批量端点)
{
    ShoppingListStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> created{false};
    QString createdName;
    bool completeMsgSeen = false;
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListCreated, [&](const QString& name) {
        createdName = name;
        created = true;
    });
    QObject::connect(&vm, &ShoppingListViewModel::batchAddComplete, [&]() { completeMsgSeen = true; });

    QVariantList missing;
    missing.append(QVariantMap{{"name", "番茄"}, {"quantity", 4.0}, {"unit", "个"}});
    missing.append(QVariantMap{{"name", "鸡蛋"}, {"quantity", 2.0}, {"unit", "个"}});

    vm.createListFromRecipe("番茄炒蛋", missing);

    ASSERT_TRUE(waitUntil(created)) << "shoppingListCreated 超时";
    EXPECT_EQ(stub.createReqCount.load(), 1) << "复合建单应只发一次创建请求";
    EXPECT_EQ(stub.batchAddReqCount.load(), 0) << "createListFromRecipe 不应再调用批量添加端点";
    EXPECT_EQ(stub.detailReqCount.load(), 0) << "创建响应已含条目，不应二次拉详情";
    EXPECT_EQ(createdName, QStringLiteral("番茄炒蛋"));
    EXPECT_TRUE(completeMsgSeen) << "成功应发 batchAddComplete（推荐卡片反馈路径）";

    // currentList 来自创建响应且含全部条目
    EXPECT_EQ(vm.currentList()["id"].toInt(), 21);
    EXPECT_EQ(vm.currentList()["items"].toList().size(), 2);

    // 请求体确实携带 items（服务端复合入口的契约）
    const std::string body = stub.createBodyCopy();
    EXPECT_NE(body.find("\"items\""), std::string::npos) << "请求体应含 items 字段: " << body;
    EXPECT_NE(body.find("番茄"), std::string::npos) << "请求体应含条目: " << body;
}

// ==================== S5：deleteShoppingListItem 成功后本地移除（v2.18） ====================
TEST_F(ShoppingListVmTest, 删除条目成功后本地移除且单请求)
{
    ShoppingListStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady));
    ASSERT_EQ(vm.currentList()["items"].toList().size(), 1);

    std::atomic<bool> updated{false};
    QObject::connect(&vm, &ShoppingListViewModel::itemUpdated, [&]() { updated = true; });

    vm.deleteShoppingListItem(11, 501);

    ASSERT_TRUE(waitUntil(updated)) << "itemUpdated 超时";
    EXPECT_EQ(stub.deleteItemReqCount.load(), 1) << "删除应只发一次请求";
    EXPECT_TRUE(vm.currentList()["items"].toList().isEmpty()) << "成功后本地移除该条目";
    EXPECT_FALSE(vm.isLoading());
}

// ==================== S6：跨条目连点按序送达且不丢更新 ====================
TEST_F(ShoppingListVmTest, 跨条目连点按序送达且不丢更新)
{
    ShoppingListStubServer stub;
    stub.detailItemCount = 3;
    stub.updateItemDelayMs = 200;   // 慢响应：保证 b、c 的点击落在 a 请求在途窗口内
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady)) << "详情加载超时";

    // 连点三个条目（a=501 → b=502 → c=503）
    vm.updateShoppingListItem(11, 501, true);
    vm.updateShoppingListItem(11, 502, true);
    vm.updateShoppingListItem(11, 503, true);

    // 乐观即时上屏：三次点击立即反映到本地（不等服务端确认）
    {
        const QVariantList items = vm.currentList()["items"].toList();
        EXPECT_TRUE(itemChecked(items, 501)) << "点击后应立即乐观显示勾选";
        EXPECT_TRUE(itemChecked(items, 502));
        EXPECT_TRUE(itemChecked(items, 503));
    }

    // 等串行队列全部送达：3 个 PATCH 且无在途
    ASSERT_TRUE(waitUntil([&]() {
        return stub.updateItemReqCount.load() == 3 && !vm.isLoading();
    })) << "串行队列未在超时内排空";

    // 关键断言：三个条目各自送达、按序、状态正确（旧单槽实现会丢 b）
    const auto reqs = stub.updateReqsCopy();
    ASSERT_EQ(reqs.size(), 3u);
    EXPECT_EQ(reqs[0], std::make_pair(501, true));
    EXPECT_EQ(reqs[1], std::make_pair(502, true));
    EXPECT_EQ(reqs[2], std::make_pair(503, true));

    pumpEvents(200);   // 观察窗口：确认无多余补发
    EXPECT_EQ(stub.updateItemReqCount.load(), 3);
    const QVariantList items = vm.currentList()["items"].toList();
    EXPECT_TRUE(itemChecked(items, 501));
    EXPECT_TRUE(itemChecked(items, 502));
    EXPECT_TRUE(itemChecked(items, 503));
}

// ==================== S7：同条目连点末态合并、陈旧 ack 不回写 ====================
TEST_F(ShoppingListVmTest, 同条目连点合并末态且陈旧确认不回写)
{
    ShoppingListStubServer stub;
    stub.updateItemDelayMs = 200;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady)) << "详情加载超时";

    // 追踪 501 每次本地变更时的勾选态（锁"陈旧 ack 不回写"）
    std::vector<bool> seen501;
    QObject::connect(&vm, &ShoppingListViewModel::currentListChanged, [&]() {
        const QVariantList items = vm.currentList()["items"].toList();
        for (const auto& v : items) {
            if (v.toMap()["id"].toInt() == 501) {
                seen501.push_back(v.toMap()["checked"].toBool());
                break;
            }
        }
    });

    vm.updateShoppingListItem(11, 501, true);
    vm.updateShoppingListItem(11, 501, false);   // 在途窗口内改主意

    ASSERT_TRUE(waitUntil([&]() {
        return stub.updateItemReqCount.load() == 2 && !vm.isLoading();
    })) << "同条目合并补发超时";

    // 两次请求：首发的 true + 合并后的末态 false
    const auto reqs = stub.updateReqsCopy();
    ASSERT_EQ(reqs.size(), 2u);
    EXPECT_EQ(reqs[0], std::make_pair(501, true));
    EXPECT_EQ(reqs[1], std::make_pair(501, false));

    // 末态
    EXPECT_FALSE(itemChecked(vm.currentList()["items"].toList(), 501));
    EXPECT_FALSE(vm.isLoading());

    // 陈旧 ack 抑制：一旦本地出现 false（用户改主意），之后不得再回写 true
    bool sawFalse = false, sawTrue = false;
    for (bool v : seen501) {
        if (!v) {
            sawFalse = true;
        } else {
            sawTrue = true;
            EXPECT_FALSE(sawFalse) << "陈旧 ack 不得把已取消的勾选回写成 true";
        }
    }
    EXPECT_TRUE(sawTrue) << "应观察到乐观勾选";
    EXPECT_TRUE(sawFalse) << "应观察到乐观取消";
}

// ==================== S8：单条失败回滚且队列继续 ====================
TEST_F(ShoppingListVmTest, 单条失败回滚且队列继续)
{
    ShoppingListStubServer stub;
    stub.detailItemCount = 3;
    stub.updateItemDelayMs = 100;
    stub.failUpdateItemId = 502;   // 中间条目失败
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady)) << "详情加载超时";

    std::atomic<bool> errorSeen{false};
    QObject::connect(&vm, &ShoppingListViewModel::errorOccurred, [&](const QString&) { errorSeen = true; });

    vm.updateShoppingListItem(11, 501, true);
    vm.updateShoppingListItem(11, 502, true);
    vm.updateShoppingListItem(11, 503, true);

    ASSERT_TRUE(waitUntil([&]() {
        return stub.updateItemReqCount.load() == 3 && !vm.isLoading();
    })) << "失败续跑超时";

    EXPECT_TRUE(errorSeen) << "失败必须上抛 errorOccurred";

    const auto reqs = stub.updateReqsCopy();
    ASSERT_EQ(reqs.size(), 3u);
    EXPECT_EQ(reqs[0], std::make_pair(501, true));
    EXPECT_EQ(reqs[1], std::make_pair(502, true));
    EXPECT_EQ(reqs[2], std::make_pair(503, true)) << "失败条目不得卡住队列";

    const QVariantList items = vm.currentList()["items"].toList();
    EXPECT_TRUE(itemChecked(items, 501));
    EXPECT_FALSE(itemChecked(items, 502)) << "失败条目应回滚到已确认态（false）";
    EXPECT_TRUE(itemChecked(items, 503));
}

// ==================== S9：clearAll 中断串行队列 ====================
TEST_F(ShoppingListVmTest, clearAll中断队列不再补发)
{
    ShoppingListStubServer stub;
    stub.detailItemCount = 3;
    stub.updateItemDelayMs = 200;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady)) << "详情加载超时";

    vm.updateShoppingListItem(11, 501, true);   // 在途
    vm.updateShoppingListItem(11, 502, true);   // 入队待发

    vm.clearAll();

    ASSERT_TRUE(waitUntil([&]() { return !vm.isLoading(); })) << "在途回调未配平";
    pumpEvents(300);   // 观察窗口：在途返回后不得补发已清队列
    EXPECT_EQ(stub.updateItemReqCount.load(), 1) << "clearAll 后不得补发已清队列";
    EXPECT_TRUE(vm.currentList().isEmpty());
}

// ==================== S10：删除剔除同条目待发勾选 ====================
TEST_F(ShoppingListVmTest, 删除剔除同条目待发勾选)
{
    ShoppingListStubServer stub;
    stub.detailItemCount = 3;
    stub.updateItemDelayMs = 200;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));

    ShoppingListViewModel vm(&api);

    std::atomic<bool> detailReady{false};
    QObject::connect(&vm, &ShoppingListViewModel::shoppingListDetailReady, [&]() { detailReady = true; });
    vm.loadShoppingListDetail(11);
    ASSERT_TRUE(waitUntil(detailReady)) << "详情加载超时";

    vm.updateShoppingListItem(11, 501, true);   // 在途
    vm.updateShoppingListItem(11, 502, true);   // 入队待发
    vm.deleteShoppingListItem(11, 502);         // 删除 502：应剔除其待发勾选

    ASSERT_TRUE(waitUntil([&]() {
        return stub.deleteItemReqCount.load() == 1
            && stub.updateItemReqCount.load() == 1
            && !vm.isLoading();
    })) << "删除与在途勾选未收敛";

    pumpEvents(300);   // 观察窗口：在途勾选返回后不得向已删条目补发 PATCH
    EXPECT_EQ(stub.updateItemReqCount.load(), 1) << "不得向已删条目补发勾选";
    EXPECT_EQ(stub.deleteItemReqCount.load(), 1);

    const QVariantList items = vm.currentList()["items"].toList();
    EXPECT_TRUE(itemChecked(items, 501));
    EXPECT_FALSE(itemPresent(items, 502)) << "删除成功后本地移除";
    EXPECT_FALSE(itemChecked(items, 503));
}

} // namespace
