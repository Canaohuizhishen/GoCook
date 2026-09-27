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
//
// 批量移动重构回归用例（客户端计数器聚合 → 服务端单请求原子更新）：
//   T5  batchMoveFavorites 一次请求发 favoriteMoved，请求体携带全部 id
//   T6  batchMoveFavorites 失败只发 favoriteOperationFailed（不发 favoriteMoved）
//   T7  moveFavorite 单条=N=1 复用批量端点
//   T8  batchRemoveFavorites 空列表守卫：不发请求、无信号（服务端空数组 400 对现客户端不可达）
//   T9  batchRemoveFavorites 非空列表一次请求发 favoriteRemoved，请求体携带全部 id
//   T10 batchRemoveFavorites 失败只发 favoriteOperationFailed（不发 favoriteRemoved）
//
// 搜索轮次守卫回归用例（搜索补 RequestEpoch 轮次作废；对齐库存“最新意图必达”语义）：
//   S1  在途期间换词：旧词迟到响应被静默丢弃，结果保留新词（不加守卫时旧词会覆盖新词）
//   S2  resetSearch 作废在途：清空后旧响应不得再落数据，加载标记复位
//
// 登出残留与串台回归用例（「登出 → 游客进页」自动化编码；两页对游客可见后补会话快照）：
//   M1  clearMyContent 清空我的投稿/我的评论列表与分页状态（登出清理——残留会让上一账号数据直接可见）
//   M2  登出后游客载入被守卫拦截：不发请求、列表保持空、error="请先登录"
//   M3/M4  会话切换后在途响应被丢弃：迟到响应不得落入已清空的我的投稿/我的评论列表
//   M5  deleteRecipe 失败回滚不得在会话切换后复活旧列表

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

// 轮询等待计数器到达目标值（“请求已到达桩”判据；配合桩内延迟构造“在途”窗口）
bool waitForCount(const std::atomic<int>& count, int target, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (count.load() < target) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (timer.elapsed() > timeoutMs)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

// 桩服务：收藏列表 / 收藏分组 / 收藏写操作 / 搜索 / 推荐（换一批）/ 我的投稿与我的评论（含删除菜谱）
class FavoriteStubServer {
public:
    // 请求流水一条记录（收藏列表请求用 page/size/group；搜索请求用 page/size/keyword）
    struct ListReq {
        int page = 0;
        int size = 0;
        std::string group;
        std::string keyword;
    };

    // 推荐请求流水（seedPresent 区分“未携带 seed 参数”与“携带 seed=0”）
    struct RecommendReq {
        int page = 0;
        int size = 0;
        unsigned int seed = 0;
        bool seedPresent = false;
    };

    std::atomic<int> favoritesListReqCount{0};
    std::atomic<int> groupsReqCount{0};
    std::atomic<int> favoriteWriteReqCount{0};
    std::atomic<int> favoriteWriteStatus{200}; // 可切换：200 成功 / 500 失败
    std::atomic<int> recommendReqCount{0};

    // M 系列（我的投稿/我的评论）：请求计数 + 可切换延迟（模拟“在途”窗口）与删除状态
    std::atomic<int> myRecipesReqCount{0};
    std::atomic<int> myRatingsReqCount{0};
    std::atomic<int> deleteRecipeReqCount{0};
    std::atomic<int> myRecipesDelayMs{0};
    std::atomic<int> myRatingsDelayMs{0};
    std::atomic<int> deleteRecipeDelayMs{0};
    std::atomic<int> deleteRecipeStatus{200}; // 可切换：200 成功 / 500 失败

    // 最近一次收藏写请求体（PATCH /favorites/batch；单条/批量移动共用，用于断言 N=1 与全量 id）
    std::string lastFavoriteWriteBody()
    {
        std::lock_guard<std::mutex> lk(writeMx);
        return lastWriteBody;
    }

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
    RecommendReq lastRecommendReq()
    {
        std::lock_guard<std::mutex> lk(reqMx);
        return recommendReqs.empty() ? RecommendReq{} : recommendReqs.back();
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
        // 批量更新收藏（PATCH /favorites/batch）：单条移动与批量移动共用同一端点
        svr.Patch("/api/users/me/favorites/batch", [this](const httplib::Request& req, httplib::Response& res) {
            favoriteWriteReqCount++;
            {
                std::lock_guard<std::mutex> lk(writeMx);
                lastWriteBody = req.body;
            }
            res.status = favoriteWriteStatus.load();
            res.set_content(R"({"message":"ok"})", "application/json");
        });
        // 批量删除收藏（POST /favorites/batch）：供 T8 反向断言——空列表守卫下不应有任何请求到达
        svr.Post("/api/users/me/favorites/batch", [this](const httplib::Request& req, httplib::Response& res) {
            favoriteWriteReqCount++;
            {
                std::lock_guard<std::mutex> lk(writeMx);
                lastWriteBody = req.body;
            }
            res.status = favoriteWriteStatus.load();
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
            // 竞态用例用：含“慢”的关键词延迟返回，模拟旧词响应晚到新词之后
            if (keyword.find("慢") != std::string::npos)
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // total_pages 固定 2：保证首屏 hasMore=true 以允许续页请求；
            // data 回显 keyword 作 name：竞态用例据此断言“展示的是哪个词的结果”
            res.status = 200;
            res.set_content(
                "{\"pagination\":{\"page\":" + std::to_string(page) +
                ",\"size\":" + std::to_string(size) +
                ",\"total\":10,\"total_pages\":2},"
                "\"data\":[{\"id\":1,\"name\":\"" + keyword + "\"}]}",
                "application/json");
        });

        // 推荐（换一批）：记录 page/size/seed 供参数断言；返回最小推荐体（1 条占位）
        svr.Get("/api/recipes/recommend", [this](const httplib::Request& req, httplib::Response& res) {
            RecommendReq rec;
            rec.page = std::stoi(req.get_param_value("page"));
            rec.size = std::stoi(req.get_param_value("size"));
            rec.seedPresent = req.has_param("seed");
            if (rec.seedPresent)
                rec.seed = static_cast<unsigned int>(std::stoul(req.get_param_value("seed")));
            recommendReqCount++;
            {
                std::lock_guard<std::mutex> lk(reqMx);
                recommendReqs.push_back(rec);
            }
            res.status = 200;
            res.set_content(
                "{\"health_filter_applied\":false,"
                "\"pagination\":{\"page\":" + std::to_string(rec.page) +
                ",\"size\":" + std::to_string(rec.size) + ",\"total\":1,\"total_pages\":1},"
                "\"data\":[{\"id\":1,\"name\":\"清蒸鲈鱼\",\"match_score\":0.5,"
                "\"health_notice\":\"\",\"match_status\":{\"available_ingredients\":[],\"missing_ingredients\":[]}}]}",
                "application/json");
        });

        // ---- M 系列：我的投稿 / 我的评论 / 删除菜谱 ----
        svr.Get("/api/recipes/my", [this](const httplib::Request& req, httplib::Response& res) {
            myRecipesReqCount++;
            if (myRecipesDelayMs.load() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(myRecipesDelayMs.load()));
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            // total_pages 固定 2：保证首屏 hasMore=true，M1 据此断言 clearMyContent 将其复位
            res.status = 200;
            res.set_content(
                "{\"pagination\":{\"page\":" + std::to_string(page) +
                ",\"size\":" + std::to_string(size) +
                ",\"total\":2,\"total_pages\":2},"
                "\"data\":[{\"id\":7,\"name\":\"红烧排骨\",\"status\":\"approved\","
                "\"submitted_at\":\"2026-09-01 10:00:00\",\"updated_at\":\"2026-09-02 10:00:00\"}]}",
                "application/json");
        });
        svr.Get("/api/users/me/ratings", [this](const httplib::Request& req, httplib::Response& res) {
            myRatingsReqCount++;
            if (myRatingsDelayMs.load() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(myRatingsDelayMs.load()));
            const int page = std::stoi(req.get_param_value("page"));
            const int size = std::stoi(req.get_param_value("size"));
            res.status = 200;
            res.set_content(
                "{\"pagination\":{\"page\":" + std::to_string(page) +
                ",\"size\":" + std::to_string(size) +
                ",\"total\":2,\"total_pages\":2},"
                "\"data\":[{\"rating_id\":1,\"recipe_id\":2,\"recipe_name\":\"番茄炒蛋\","
                "\"rating\":5,\"comment\":\"好吃\","
                "\"created_at\":\"2026-09-01 10:00:00\",\"updated_at\":\"2026-09-01 10:00:00\"}]}",
                "application/json");
        });
        svr.Delete(R"(/api/recipes/(\d+))", [this](const httplib::Request&, httplib::Response& res) {
            deleteRecipeReqCount++;
            if (deleteRecipeDelayMs.load() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(deleteRecipeDelayMs.load()));
            res.status = deleteRecipeStatus.load();
            res.set_content(R"({"message":"ok"})", "application/json");
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
    std::mutex writeMx;              // 保护收藏写请求体（服务端线程写入，测试线程读取）
    std::string lastWriteBody;
    std::mutex reqMx;                // 保护三组请求流水（服务端线程写入，测试线程读取）
    std::vector<ListReq> favoriteListReqs;
    std::vector<ListReq> searchReqs;
    std::vector<RecommendReq> recommendReqs;
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

    vm.loadFavorites(1, "");
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

    vm.loadFavorites(1, "");
    ASSERT_TRUE(waitUntil(favoritesChanged)) << "加载全部收藏超时";
    EXPECT_EQ(vm.favoritesAllCount(), 3);

    // 切到“家常菜”分组（服务端该分组 total=1）：旧实现会把「全部」计数覆写为筛选总数
    favoritesChanged = false;
    vm.loadFavorites(1, "家常菜");
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
    vm.loadFavorites(1, "默认收藏夹", 1);
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

// ==================== T5：batchMoveFavorites 单请求原子化（替代原客户端计数器聚合） ====================
TEST_F(RecipeVmTest, batchMoveFavorites一次请求发favoriteMoved)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> moved{false};
    std::atomic<bool> failed{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteMoved, [&]() { moved = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed, [&](const QString&) { failed = true; });

    QVariantList ids;
    ids << 11 << 12;
    vm.batchMoveFavorites(ids, 2);

    ASSERT_TRUE(waitUntil(moved)) << "favoriteMoved 超时";
    EXPECT_FALSE(failed.load()) << "全部成功不得发失败信号";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1) << "两个 id 必须聚合为一次请求（原实现发 N 次）";

    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(stub.lastFavoriteWriteBody()));
    const QJsonArray bodyIds = doc.object()["favorite_ids"].toArray();
    ASSERT_EQ(bodyIds.size(), 2) << "请求体应携带全部收藏 id";
    EXPECT_EQ(bodyIds[0].toInt(), 11);
    EXPECT_EQ(bodyIds[1].toInt(), 12);
    EXPECT_EQ(doc.object()["group_id"].toInt(), 2);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteMoved, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== T6：batchMoveFavorites 失败=原子失败（不发 favoriteMoved） ====================
TEST_F(RecipeVmTest, batchMoveFavorites失败只发favoriteOperationFailed)
{
    FavoriteStubServer stub;
    stub.favoriteWriteStatus = 500;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> moved{false};
    std::atomic<bool> failed{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteMoved, [&]() { moved = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed, [&](const QString&) { failed = true; });

    QVariantList ids;
    ids << 11 << 12;
    vm.batchMoveFavorites(ids, 2);

    ASSERT_TRUE(waitUntil(failed)) << "favoriteOperationFailed 超时";
    EXPECT_FALSE(moved.load()) << "失败不得发 favoriteMoved（旧实现部分失败仍会发，页面误判）";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteMoved, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== T7：moveFavorite 单条=N=1 复用批量端点 ====================
TEST_F(RecipeVmTest, moveFavorite单条请求N等于1)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> moved{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteMoved, [&]() { moved = true; });

    vm.moveFavorite(21, 3);

    ASSERT_TRUE(waitUntil(moved)) << "favoriteMoved 超时";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1);

    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(stub.lastFavoriteWriteBody()));
    const QJsonArray bodyIds = doc.object()["favorite_ids"].toArray();
    ASSERT_EQ(bodyIds.size(), 1) << "单条移动应携带单元素 favorite_ids（N=1 复用）";
    EXPECT_EQ(bodyIds[0].toInt(), 21);
    EXPECT_EQ(doc.object()["group_id"].toInt(), 3);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteMoved, nullptr, nullptr);
}

// ==================== T8：batchRemoveFavorites 空列表守卫（不发请求、无信号） ====================
TEST_F(RecipeVmTest, batchRemoveFavorites空列表不发请求)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> removed{false};
    std::atomic<bool> failed{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteRemoved, [&]() { removed = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed, [&](const QString&) { failed = true; });

    vm.batchRemoveFavorites(QVariantList{});

    // 空列表守卫应在发出请求前返回：给出与本地网络请求同量级的事件循环窗口，
    // 断言零请求、零信号（若守卫被破坏，请求会命中桩并被计数，失败信号也会置位）
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 200) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 0) << "空列表必须不发请求（服务端 400 应对现客户端不可达）";
    EXPECT_FALSE(removed.load());
    EXPECT_FALSE(failed.load());

    QObject::disconnect(&vm, &RecipeViewModel::favoriteRemoved, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== T9：batchRemoveFavorites 非空列表=单请求成功（正路径） ====================
TEST_F(RecipeVmTest, batchRemoveFavorites非空列表一次请求发favoriteRemoved)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> removed{false};
    std::atomic<bool> failed{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteRemoved, [&]() { removed = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed, [&](const QString&) { failed = true; });

    QVariantList ids;
    ids << 11 << 12;
    vm.batchRemoveFavorites(ids);

    ASSERT_TRUE(waitUntil(removed)) << "favoriteRemoved 超时";
    EXPECT_FALSE(failed.load()) << "全部成功不得发失败信号";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1) << "两个 id 必须聚合为一次请求";

    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(stub.lastFavoriteWriteBody()));
    const QJsonArray bodyIds = doc.object()["favorite_ids"].toArray();
    ASSERT_EQ(bodyIds.size(), 2) << "请求体应携带全部收藏 id";
    EXPECT_EQ(bodyIds[0].toInt(), 11);
    EXPECT_EQ(bodyIds[1].toInt(), 12);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteRemoved, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== T10：batchRemoveFavorites 失败只发 favoriteOperationFailed ====================
TEST_F(RecipeVmTest, batchRemoveFavorites失败只发favoriteOperationFailed)
{
    FavoriteStubServer stub;
    stub.favoriteWriteStatus = 500;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> removed{false};
    std::atomic<bool> failed{false};
    QObject::connect(&vm, &RecipeViewModel::favoriteRemoved, [&]() { removed = true; });
    QObject::connect(&vm, &RecipeViewModel::favoriteOperationFailed, [&](const QString&) { failed = true; });

    QVariantList ids;
    ids << 11 << 12;
    vm.batchRemoveFavorites(ids);

    ASSERT_TRUE(waitUntil(failed)) << "favoriteOperationFailed 超时";
    EXPECT_FALSE(removed.load()) << "失败不得发 favoriteRemoved";
    EXPECT_EQ(stub.favoriteWriteReqCount.load(), 1);

    QObject::disconnect(&vm, &RecipeViewModel::favoriteRemoved, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::favoriteOperationFailed, nullptr, nullptr);
}

// ==================== S1：在途期间换词——旧词迟到响应被静默丢弃，结果保留新词 ====================
TEST_F(RecipeVmTest, 搜索在途期间换词旧响应被丢弃新词必达)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> fastLanded{false};
    std::atomic<bool> errorEmitted{false};
    QObject::connect(&vm, &RecipeViewModel::searchResultsChanged, [&]() {
        if (!vm.searchResults().isEmpty())
            fastLanded = true;
    });
    QObject::connect(&vm, &RecipeViewModel::searchErrorOccurred, [&](const QString&) { errorEmitted = true; });

    // 先发“慢词”（服务端延迟 500ms），在途期间再发“快词”（立即返回）：
    // 新词开启新一轮轮次 → 慢词票据失效，其迟到响应必须被静默丢弃（不得覆盖快词结果）
    vm.searchRecipes(QStringLiteral("慢词"), 1, 5);
    vm.searchRecipes(QStringLiteral("快词"), 1, 5);
    ASSERT_TRUE(waitUntil(fastLanded)) << "快词响应超时";

    // 等到慢词响应窗口（500ms）过去后再断言：展示的仍是快词结果
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 900) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_EQ(vm.searchResults().size(), 1) << "慢词迟到响应不得再落数据";
    EXPECT_EQ(vm.searchResults()[0].toMap()["name"].toString(), QStringLiteral("快词"))
        << "结果必须保留最新词（不加守卫时慢词会覆盖快词）";
    EXPECT_FALSE(errorEmitted.load()) << "过期响应丢弃不得报错";
    EXPECT_FALSE(vm.searchLoading()) << "加载标记应由新词请求收尾复位";

    QObject::disconnect(&vm, &RecipeViewModel::searchResultsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::searchErrorOccurred, nullptr, nullptr);
}

// ==================== S2：resetSearch 作废在途——清空后旧响应不得再落数据 ====================
TEST_F(RecipeVmTest, resetSearch作废在途旧响应不再落数据)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);

    vm.searchRecipes(QStringLiteral("慢词"), 1, 5);   // 服务端延迟 500ms
    vm.resetSearch();                                  // 清空并作废在途

    // 等到慢词响应窗口过去后断言：结果保持为空、空态标记保持未执行、加载标记已复位
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 900) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(vm.searchResults().isEmpty()) << "resetSearch 后旧响应不得回填结果";
    EXPECT_FALSE(vm.searchPerformed()) << "空态标记不得被旧响应置真";
    EXPECT_FALSE(vm.searchLoading()) << "resetSearch 应复位加载标记（在途请求已作废）";
}

// ==================== M1：clearMyContent 清空我的投稿/我的评论列表与分页状态 ====================
TEST_F(RecipeVmTest, clearMyContent清空两列表与分页状态)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    // 注：入口清空（page==1）同样会发 Changed 信号，用"非空"条件区分加载完成（对齐 S1 写法）
    std::atomic<bool> recipesLoaded{false};
    std::atomic<bool> ratingsLoaded{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesChanged, [&]() {
        if (!vm.myRecipes().isEmpty())
            recipesLoaded = true;
    });
    QObject::connect(&vm, &RecipeViewModel::myRatingsChanged, [&]() {
        if (!vm.myRatings().isEmpty())
            ratingsLoaded = true;
    });

    vm.loadMyRecipes(1, "");
    ASSERT_TRUE(waitUntil(recipesLoaded)) << "加载我的投稿超时";
    ASSERT_EQ(vm.myRecipes().size(), 1);
    ASSERT_TRUE(vm.myRecipesHasMore()) << "桩 total_pages=2，首屏应有下一页";

    vm.loadMyRatings();
    ASSERT_TRUE(waitUntil(ratingsLoaded)) << "加载我的评论超时";
    ASSERT_EQ(vm.myRatings().size(), 1);
    ASSERT_TRUE(vm.myRatingsHasMore()) << "桩 total_pages=2，首屏应有下一页";

    std::atomic<bool> recipesHasMoreChanged{false};
    std::atomic<bool> ratingsHasMoreChanged{false};
    std::atomic<bool> recipesLoadingChanged{false};
    std::atomic<bool> ratingsLoadingChanged{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesHasMoreChanged, [&]() { recipesHasMoreChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::myRatingsHasMoreChanged, [&]() { ratingsHasMoreChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::myRecipesLoadingChanged, [&]() { recipesLoadingChanged = true; });
    QObject::connect(&vm, &RecipeViewModel::myRatingsLoadingChanged, [&]() { ratingsLoadingChanged = true; });

    std::atomic<bool> recipesCleared{false};
    std::atomic<bool> ratingsCleared{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesChanged, [&]() { recipesCleared = true; });
    QObject::connect(&vm, &RecipeViewModel::myRatingsChanged, [&]() { ratingsCleared = true; });

    vm.clearMyContent();

    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "我的投稿必须清空（登出残留防护）";
    EXPECT_TRUE(vm.myRatings().isEmpty()) << "我的评论必须清空（登出残留防护）";
    EXPECT_FALSE(vm.myRecipesHasMore()) << "分页状态必须复位";
    EXPECT_FALSE(vm.myRatingsHasMore()) << "分页状态必须复位";
    EXPECT_TRUE(recipesCleared.load()) << "清空必须发 myRecipesChanged";
    EXPECT_TRUE(ratingsCleared.load()) << "清空必须发 myRatingsChanged";
    EXPECT_TRUE(recipesHasMoreChanged.load());
    EXPECT_TRUE(ratingsHasMoreChanged.load());
    EXPECT_TRUE(recipesLoadingChanged.load());
    EXPECT_TRUE(ratingsLoadingChanged.load());

    QObject::disconnect(&vm, &RecipeViewModel::myRecipesChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRatingsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRecipesHasMoreChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRatingsHasMoreChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRecipesLoadingChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRatingsLoadingChanged, nullptr, nullptr);
}

// ==================== M2：登出 → 游客进页——守卫拦截不发请求、无残留 ====================
TEST_F(RecipeVmTest, 登出后游客载入被守卫拦截且无残留)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    // 注：入口清空（page==1）同样会发 Changed 信号，用"非空"条件区分加载完成（对齐 S1 写法）
    std::atomic<bool> recipesLoaded{false};
    std::atomic<bool> ratingsLoaded{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesChanged, [&]() {
        if (!vm.myRecipes().isEmpty())
            recipesLoaded = true;
    });
    QObject::connect(&vm, &RecipeViewModel::myRatingsChanged, [&]() {
        if (!vm.myRatings().isEmpty())
            ratingsLoaded = true;
    });

    // 登录态：先加载出数据
    vm.loadMyRecipes(1, "");
    ASSERT_TRUE(waitUntil(recipesLoaded)) << "加载我的投稿超时";
    ASSERT_EQ(vm.myRecipes().size(), 1);
    vm.loadMyRatings();
    ASSERT_TRUE(waitUntil(ratingsLoaded)) << "加载我的评论超时";
    ASSERT_EQ(vm.myRatings().size(), 1);

    // 登出：token 清空（AuthViewModel::logout）+ 会话结束清理（main.cpp sessionEnded 接线等价调用）
    api.setToken(QString());
    vm.clearMyContent();
    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "登出不得留下上一账号的我的投稿";
    EXPECT_TRUE(vm.myRatings().isEmpty()) << "登出不得留下上一账号的我的评论";

    // 游客进页（等价页面 Component.onCompleted 的 loadMyRecipes/loadMyRatings）：
    // Silent 守卫拦下请求（不发）、回调"请先登录"、列表保持空
    std::atomic<bool> errorFlag{false};
    std::string lastError;
    QObject::connect(&vm, &RecipeViewModel::errorOccurred, [&](const QString& e) {
        lastError = e.toStdString();
        errorFlag = true;
    });
    const int recipesReqsBefore = stub.myRecipesReqCount.load();
    const int ratingsReqsBefore = stub.myRatingsReqCount.load();

    errorFlag = false;
    vm.loadMyRecipes(1, "");
    ASSERT_TRUE(waitUntil(errorFlag)) << "守卫拦截回调超时";
    EXPECT_EQ(lastError, "请先登录");
    EXPECT_EQ(stub.myRecipesReqCount.load(), recipesReqsBefore) << "游客载入不得发出请求";
    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "游客载入不得落数据";
    EXPECT_FALSE(vm.myRecipesLoading()) << "加载标记必须复位";

    errorFlag = false;
    vm.loadMyRatings();
    ASSERT_TRUE(waitUntil(errorFlag)) << "守卫拦截回调超时";
    EXPECT_EQ(lastError, "请先登录");
    EXPECT_EQ(stub.myRatingsReqCount.load(), ratingsReqsBefore) << "游客载入不得发出请求";
    EXPECT_TRUE(vm.myRatings().isEmpty()) << "游客载入不得落数据";
    EXPECT_FALSE(vm.myRatingsLoading()) << "加载标记必须复位";

    QObject::disconnect(&vm, &RecipeViewModel::myRecipesChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::myRatingsChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::errorOccurred, nullptr, nullptr);
}

// ==================== M3：会话切换后 loadMyRecipes 过期响应被丢弃 ====================
TEST_F(RecipeVmTest, loadMyRecipes会话切换后过期响应被丢弃)
{
    FavoriteStubServer stub;
    stub.myRecipesDelayMs = 400;   // 请求到达桩后延迟响应，制造"在途"窗口
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> loadingReset{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesLoadingChanged, [&]() {
        if (!vm.myRecipesLoading())
            loadingReset = true;
    });

    vm.loadMyRecipes(1, "");
    ASSERT_TRUE(waitForCount(stub.myRecipesReqCount, 1)) << "请求未到达桩";

    // 登出：会话切换发生在响应落地之前
    api.setToken(QString());

    // 迟到响应到达 → 会话快照判废；兜底复位加载标记可观测，据此等待响应已被处理
    ASSERT_TRUE(waitUntil(loadingReset)) << "过期响应未按预期丢弃（加载标记未复位）";
    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "过期响应不得落入已登出会话的列表";

    QObject::disconnect(&vm, &RecipeViewModel::myRecipesLoadingChanged, nullptr, nullptr);
}

// ==================== M4：会话切换后 loadMyRatings 过期响应被丢弃 ====================
TEST_F(RecipeVmTest, loadMyRatings会话切换后过期响应被丢弃)
{
    FavoriteStubServer stub;
    stub.myRatingsDelayMs = 400;   // 请求到达桩后延迟响应，制造"在途"窗口
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> loadingReset{false};
    QObject::connect(&vm, &RecipeViewModel::myRatingsLoadingChanged, [&]() {
        if (!vm.myRatingsLoading())
            loadingReset = true;
    });

    vm.loadMyRatings();
    ASSERT_TRUE(waitForCount(stub.myRatingsReqCount, 1)) << "请求未到达桩";

    // 登出：会话切换发生在响应落地之前
    api.setToken(QString());

    ASSERT_TRUE(waitUntil(loadingReset)) << "过期响应未按预期丢弃（加载标记未复位）";
    EXPECT_TRUE(vm.myRatings().isEmpty()) << "过期响应不得落入已登出会话的列表";

    QObject::disconnect(&vm, &RecipeViewModel::myRatingsLoadingChanged, nullptr, nullptr);
}

// ==================== M5：deleteRecipe 失败回滚不得在会话切换后复活旧列表 ====================
TEST_F(RecipeVmTest, deleteRecipe会话切换后失败不回滚复活旧列表)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> recipesLoaded{false};
    std::atomic<bool> deleteFailed{false};
    QObject::connect(&vm, &RecipeViewModel::myRecipesChanged, [&]() {
        if (!vm.myRecipes().isEmpty())
            recipesLoaded = true;   // 入口清空也会发 Changed，用"非空"区分加载完成（对齐 S1 写法）
    });
    QObject::connect(&vm, &RecipeViewModel::deleteFailed, [&](const QString&) { deleteFailed = true; });

    vm.loadMyRecipes(1, "");
    ASSERT_TRUE(waitUntil(recipesLoaded)) << "加载我的投稿超时";
    ASSERT_EQ(vm.myRecipes().size(), 1);

    // 删除请求：乐观移除 + 延迟失败响应（模拟在途）
    stub.deleteRecipeStatus = 500;
    stub.deleteRecipeDelayMs = 400;
    vm.deleteRecipe(7);
    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "乐观删除应立即移除条目";
    ASSERT_TRUE(waitForCount(stub.deleteRecipeReqCount, 1)) << "删除请求未到达桩";

    // 会话切换（登出）+ 会话结束清理，均发生在失败响应落地之前
    api.setToken(QString());
    vm.clearMyContent();

    // 等失败响应落地（延迟 + 余量，期间泵事件循环）
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 400 + 400) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(vm.myRecipes().isEmpty()) << "过期失败响应不得回滚复活上一账号的列表";
    EXPECT_FALSE(deleteFailed.load()) << "过期响应不得发失败信号";
    EXPECT_FALSE(vm.myRecipesLoading()) << "过期响应不得触发重拉";

    QObject::disconnect(&vm, &RecipeViewModel::myRecipesChanged, nullptr, nullptr);
    QObject::disconnect(&vm, &RecipeViewModel::deleteFailed, nullptr, nullptr);
}

// ==================== JP1：推荐首载按批次尺寸且不携带种子 ====================
TEST_F(RecipeVmTest, 推荐首载按批次尺寸且不携带种子)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<bool> loaded{false};
    QObject::connect(&vm, &RecipeViewModel::recipesChanged, [&]() {
        if (!vm.recipes().isEmpty())
            loaded = true;   // 入口清空也会发 Changed，用“非空”区分加载完成（对齐 S1 写法）
    });

    vm.loadRecommendedRecipes(1);
    ASSERT_TRUE(waitUntil(loaded)) << "推荐加载超时";

    const auto req = stub.lastRecommendReq();
    EXPECT_EQ(req.page, 1);
    EXPECT_EQ(req.size, RecipeViewModel::kRecPageSize) << "推荐批次尺寸必须与公开列表解耦";
    EXPECT_FALSE(req.seedPresent) << "首屏不得携带 seed（确定性路径）";
    ASSERT_EQ(vm.recipes().size(), 1);
    EXPECT_EQ(vm.recipes()[0].toMap()["name"].toString(), QStringLiteral("清蒸鲈鱼"));
    EXPECT_FALSE(vm.healthFilterApplied());

    QObject::disconnect(&vm, &RecipeViewModel::recipesChanged, nullptr, nullptr);
}

// ==================== JP2：推荐换一批携带非零种子且逐次刷新 ====================
TEST_F(RecipeVmTest, 推荐换一批携带非零种子且逐次不同)
{
    FavoriteStubServer stub;
    api.setBaseUrl(QString::fromStdString(stub.baseUrl()));
    api.setToken(QStringLiteral("token-A"));

    RecipeViewModel vm(&api);
    std::atomic<int> loadedCount{0};
    QObject::connect(&vm, &RecipeViewModel::recipesChanged, [&]() {
        if (!vm.recipes().isEmpty())
            loadedCount++;
    });

    vm.shuffleRecommended();
    ASSERT_TRUE(waitForCount(loadedCount, 1)) << "首次换一批超时";
    const auto r1 = stub.lastRecommendReq();

    vm.shuffleRecommended();
    ASSERT_TRUE(waitForCount(loadedCount, 2)) << "二次换一批超时";
    const auto r2 = stub.lastRecommendReq();

    EXPECT_TRUE(r1.seedPresent) << "换一批必须携带 seed";
    EXPECT_TRUE(r2.seedPresent) << "换一批必须携带 seed";
    EXPECT_NE(r1.seed, 0u) << "换一批种子必须非零（0 保留给确定性首屏）";
    EXPECT_NE(r2.seed, 0u);
    EXPECT_EQ(r1.size, RecipeViewModel::kRecPageSize);
    EXPECT_EQ(r2.size, RecipeViewModel::kRecPageSize);
    // 两次种子必须不同（QRandomGenerator 碰撞概率 2^-32；相同即疑似种子未刷新）
    EXPECT_NE(r1.seed, r2.seed) << "两次换一批种子相同（疑似种子未刷新）";

    QObject::disconnect(&vm, &RecipeViewModel::recipesChanged, nullptr, nullptr);
}
} // namespace
