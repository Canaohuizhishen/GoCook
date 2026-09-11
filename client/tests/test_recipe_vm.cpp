// RecipeViewModel 收藏域测试
//
// 覆盖 fd39eb62 后审查发现的问题（收藏乐观状态 + 登出残留）：
//   R1  clearFavorites 同时清空收藏列表与收藏分组（登出清理——分组残留会让游客在
//       详情页看到上一账号的收藏分组，进而触发"跳过登录页后按钮变已收藏"的假状态）
//   R2  toggleFavorite 成功 → favoriteToggleSuccess(recipeId, true)
//   R3  toggleFavorite 失败（服务端 500）→ favoriteOperationFailed（页面据此回滚乐观状态）
//
// 收藏计数与续页参数修复的回归用例（「全部」计数乱变 + 续页丢参数）：
//   T1  favoritesAllCount = Σ 分组 count，且不随列表筛选查询覆盖
//   T2  删除分组后 favoritesAllCount 同步减少（乐观移除）
//   T3  loadMoreFavorites 续页沿用首屏的分组筛选与页大小
//   T4  searchNextPage 续页沿用首屏页大小

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QString>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "HttpGoCookApi.h"
#include "RecipeViewModel.h"
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

// 收藏域桩服务：收藏列表 / 收藏分组 / 收藏写操作
class FavoriteStubServer {
public:
    // 请求流水一条记录（收藏列表请求用 page/size/group；搜索请求用 page/size/keyword）
    struct ListReq {
        int page = 0;
        int size = 0;
        std::string group;
        std::string keyword;
    };

    std::atomic<int> favoritesListReqCount{0};
    std::atomic<int> groupsReqCount{0};
    std::atomic<int> favoriteWriteReqCount{0};
    std::atomic<int> favoriteWriteStatus{200}; // 可切换：200 成功 / 500 失败

    ListReq lastFavoriteListReq()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return favoriteListReqs.empty() ? ListReq{} : favoriteListReqs.back();
    }
    ListReq lastSearchReq()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return searchReqs.empty() ? ListReq{} : searchReqs.back();
    }

    FavoriteStubServer()
    {
        svr.Get("/api/users/me/favorites", [this](const httplib::Request& req, httplib::Response& res) {
            favoritesListReqCount++;
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            const std::string group = req.get_param_value("group");
            {
                std::lock_guard<std::mutex> lk(reqMx);
                favoriteListReqs.push_back({page, size, group, ""});
            }
            // 总数按筛选分组区分（模拟服务端“筛选下的总数”语义）；data 简化为固定 1 条占位，
            // 用例只断言续页请求参数与 VM 状态流转
            int total = 3;   // 全部 = 默认收藏夹(2) + 家常菜(1)
            if (group == "默认收藏夹") total = 2;
            else if (group == "家常菜") total = 1;
            else if (!group.empty()) total = 0;
            const int totalPages = (total + size - 1) / size;
            res.status = 200;
            res.set_content(
                "{\"pagination\":{\"page\":" + std::to_string(page) +
                ",\"size\":" + std::to_string(size) +
                ",\"total\":" + std::to_string(total) +
                ",\"total_pages\":" + std::to_string(totalPages) + "},"
                "\"data\":[{\"id\":11,\"recipe_id\":1,\"name\":\"番茄炒蛋\",\"group_name\":\"默认收藏夹\"}]}",
                "application/json");
        });
        svr.Get("/api/users/me/favorites/groups", [this](const httplib::Request&, httplib::Response& res) {
            groupsReqCount++;
            res.status = 200;
            res.set_content(
                R"([{"id":1,"name":"默认收藏夹","sort_order":0,"count":2},)"
                R"({"id":2,"name":"家常菜","sort_order":1,"count":1}])",
                "application/json");
        });
        svr.Post("/api/recipes/1/favorite", [this](const httplib::Request&, httplib::Response& res) {
            favoriteWriteReqCount++;
            res.status = favoriteWriteStatus.load();
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        svr.Delete(R"(/api/users/me/favorites/groups/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            res.status = 200;
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        svr.Get("/api/recipes/search", [this](const httplib::Request& req, httplib::Response& res) {
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            const std::string keyword = req.get_param_value("keyword");
            {
                std::lock_guard<std::mutex> lk(reqMx);
                searchReqs.push_back({page, size, "", keyword});
            }
            // total_pages 固定 2：保证首屏 hasMore=true 以允许续页请求；data 留空（用例不消费结果数据）
            res.status = 200;
            res.set_content(
                "{\"pagination\":{\"page\":" + std::to_string(page) +
                ",\"size\":" + std::to_string(size) +
                ",\"total\":10,\"total_pages\":2},"
                "\"data\":[]}",
                "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("FavoriteStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });

        // 就绪轮询：listen 失败只发生在子线程（主线程无法 catch），轮询 is_running()
        // 显式暴露启动失败；同时消除“端口已绑定但尚未开始监听”的竞态窗口
        for (int i = 0; i < 200; ++i) {
            if (svr.is_running())
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("FavoriteStubServer: failed to start");
    }

    ~FavoriteStubServer()
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
    std::mutex reqMx;                // 保护两个请求流水（服务端线程写入，测试线程读取）
    std::vector<ListReq> favoriteListReqs;
    std::vector<ListReq> searchReqs;
};

class RecipeVmTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        api.setMaxRetries(0);
        api.setToken(QString()); // 每个用例从未登录开始
    }

    HttpGoCookApi api;
};

// ==================== R1：clearFavorites 同时清空列表与分组 ====================
TEST_F(RecipeVmTest, clearFavorites清空列表与分组)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> groupsChanged{false};
    std::atomic<bool> favoritesChanged{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteGroupsChanged, [&]() { groupsChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::favoritesChanged, [&]() { favoritesChanged = true; });

    vm.loadFavoriteGroups();
    ASSERT_TRUE(waitUntil(groupsChanged)) << "加载分组超时";
    ASSERT_EQ(vm.favoriteGroups().size(), 2);

    vm.loadFavorites(1, 20, "");
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "加载收藏列表超时";
    ASSERT_EQ(vm.favorites().size(), 1);

    groupsChanged = false;
    vm.clearFavorites();
    EXPECT_TRUE(vm.favorites().isEmpty()) << "收藏列表必须清空";
    EXPECT_TRUE(vm.favoriteGroups().isEmpty()) << "收藏分组必须清空（登出残留防护）";
    EXPECT_TRUE(groupsChanged.load()) << "分组清空必须发 favoriteGroupsChanged";

    QObject::disconnect(&vm, &RecipeViewModel::favoriteGroupsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoritesChanged, nullptr, nullptr);
}

// ==================== R2：toggleFavorite 成功信号 ====================
TEST_F(RecipeVmTest, toggleFavorite成功发信号)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> successFlag{false};
    int gotRecipeId = -1;
    QObject::connect(&vm, &RecipeViewModel::favoriteToggleSuccess,
                     [&](int recipeId, bool) { gotRecipeId = recipeId; successFlag = true; });

    vm.toggleFavorite(1, 0);
    ASSERT_TRUE(waitUntil(successFlag)) << "收藏成功信号超时";
    EXPECT_EQ(gotRecipeId, 1);
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteToggleSuccess, nullptr, nullptr);
}

// ==================== R3：toggleFavorite 失败信号（页面据此回滚） ====================
TEST_F(RecipeVmTest, toggleFavorite失败发错误信号)
{
    FavoriteStubServer stub;
    stub.favoriteWriteStatus = 500;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> errorFlag{false};
    std::string gotError;
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed,
                     [&](const QString& e) { gotError = e.toStdString(); errorFlag = true; });

    vm.toggleFavorite(1, 0);
    ASSERT_TRUE(waitUntil(errorFlag)) << "收藏失败信号超时";
    EXPECT_FALSE(gotError.empty()) << "失败必须携带错误文案";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== T1：favoritesAllCount = Σ 分组 count 且不随筛选覆盖 ====================
TEST_F(RecipeVmTest, favoritesAllCount等于分组之和且不随筛选变化)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> groupsChanged{false};
    std::atomic<bool> favoritesChanged{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteGroupsChanged, [&]() { groupsChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::favoritesChanged, [&]() { favoritesChanged = true; });

    vm.loadFavoriteGroups();
    ASSERT_TRUE(waitUntil(groupsChanged)) << "加载分组超时";
    EXPECT_EQ(vm.favoritesAllCount(), 3) << "「全部」= 默认收藏夹(2) + 家常菜(1)";

    vm.loadFavorites(1, 20, "");
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "加载全部收藏超时";
    EXPECT_EQ(vm.favoritesAllCount(), 3);

    // 切到“家常菜”分组（服务端该分组 total=1）：旧实现会把「全部」计数覆写为筛选总数
    favoritesChanged = false;
    vm.loadFavorites(1, 20, "家常菜");
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "加载分组收藏超时";
    EXPECT_EQ(vm.favoritesAllCount(), 3) << "「全部」总数不得被筛选结果覆盖";

    QObject::disconnect(&vm, &RecipeViewModel::favoriteGroupsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoritesChanged, nullptr, nullptr);
}

// ==================== T2：删除分组后 favoritesAllCount 同步减少 ====================
TEST_F(RecipeVmTest, 删除分组后favoritesAllCount同步减少)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> groupsChanged{false};
    std::atomic<bool> deleted{false};
    std::string failMsg;
    QObject::connect(&vm, &RecipeViewModel::favoriteGroupsChanged, [&]() { groupsChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteGroupDeleted, [&]() { deleted = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed,
                     [&](const QString& e) { failMsg = e.toStdString(); });

    vm.loadFavoriteGroups();
    ASSERT_TRUE(waitUntil(groupsChanged)) << "加载分组超时";
    ASSERT_EQ(vm.favoritesAllCount(), 3);

    // 本用例同时回归防护“无体 DELETE 缺 Content-Length 被服务端 httplib 阻塞”：
    // HttpGoCookApi::sendRequest 会为无体 DELETE 补最小 JSON 体，服务端能读完并正常响应
    vm.deleteFavoriteGroup(2);   // 家常菜 count=1：乐观移除后应立即减少
    EXPECT_EQ(vm.favoritesAllCount(), 2) << "乐观删除后计数应同步减少";

    ASSERT_TRUE(waitUntil(deleted)) << "删除分组超时，失败信号消息: " << failMsg;
    EXPECT_EQ(vm.favoritesAllCount(), 2);
    EXPECT_EQ(vm.favoriteGroups().size(), 1);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteGroupsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteGroupDeleted, nullptr, nullptr);
}

// ==================== T3：loadMoreFavorites 续页沿用分组与页大小 ====================
TEST_F(RecipeVmTest, loadMoreFavorites续页沿用分组与页大小)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> favoritesChanged{false};
    QObject::connect(&vm, &RecipeViewModel::favoritesChanged, [&]() { favoritesChanged = true; });

    // size=1 + 默认收藏夹 total=2 → 两页，hasMore=true
    vm.loadFavorites(1, 1, "默认收藏夹");
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "首屏加载超时";
    ASSERT_TRUE(vm.favoritesHasMore()) << "应还有下一页";

    favoritesChanged = false;
    vm.loadMoreFavorites();
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "续页加载超时";

    const auto req = stub.lastFavoriteListReq();
    EXPECT_EQ(req.page, 2);
    EXPECT_EQ(req.size, 1) << "续页 size 必须与首屏一致（旧实现固定用 m_pageSize=30）";
    EXPECT_EQ(req.group, "默认收藏夹") << "续页必须携带当前分组筛选（旧实现丢失分组）";

    QObject::disconnect(&vm, &RecipeViewModel::favoritesChanged, nullptr, nullptr);
}

// ==================== T4：searchNextPage 续页沿用首屏页大小 ====================
TEST_F(RecipeVmTest, searchNextPage续页沿用首屏页大小)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> resultsChanged{false};
    QObject::connect(&vm, &RecipeViewModel::searchResultsChanged, [&]() { resultsChanged = true; });

    vm.searchRecipes(QStringLiteral("番茄"), 1, 5);
    ASSERT_TRUE(waitUntil(resultsChanged)) << "搜索首屏超时";
    ASSERT_TRUE(vm.searchHasMore()) << "应还有下一页";

    resultsChanged = false;
    vm.searchNextPage();
    ASSERT_TRUE(waitUntil(resultsChanged)) << "搜索续页超时";

    const auto req = stub.lastSearchReq();
    EXPECT_EQ(req.page, 2);
    EXPECT_EQ(req.size, 5) << "续页 size 必须与首屏一致（旧实现固定用 m_pageSize=30）";
    EXPECT_EQ(req.keyword, "番茄");

    QObject::disconnect(&vm, &RecipeViewModel::searchResultsChanged, nullptr, nullptr);
}
} // namespace
