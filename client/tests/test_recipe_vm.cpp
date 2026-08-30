// RecipeViewModel 收藏域测试
//
// 覆盖 fd39eb62 后审查发现的问题（收藏乐观状态 + 登出残留）：
//   R1  clearFavorites 同时清空收藏列表与收藏分组（登出清理——分组残留会让游客在
//       详情页看到上一账号的收藏分组，进而触发"跳过登录页后按钮变已收藏"的假状态）
//   R2  toggleFavorite 成功 → favoriteToggleSuccess(recipeId, true)
//   R3  toggleFavorite 失败（服务端 500）→ favoriteOperationFailed（页面据此回滚乐观状态）

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
    std::atomic<int> favoritesListReqCount{0};
    std::atomic<int> groupsReqCount{0};
    std::atomic<int> favoriteWriteReqCount{0};
    std::atomic<int> favoriteWriteStatus{200}; // 可切换：200 成功 / 500 失败

    FavoriteStubServer()
    {
        svr.Get("/api/users/me/favorites", [this](const httplib::Request&, httplib::Response& res) {
            favoritesListReqCount++;
            res.status = 200;
            res.set_content(
                R"({"pagination":{"page":1,"size":20,"total":1,"total_pages":1},)"
                R"("data":[{"id":11,"recipe_id":1,"recipe_name":"番茄炒蛋","is_favorited":true}]})",
                "application/json");
        });
        svr.Get("/api/users/me/favorites/groups", [this](const httplib::Request&, httplib::Response& res) {
            groupsReqCount++;
            res.status = 200;
            res.set_content(
                R"([{"id":1,"name":"默认收藏夹","sort_order":0,"count":1},)"
                R"({"id":2,"name":"家常菜","sort_order":1,"count":0}])",
                "application/json");
        });
        svr.Post("/api/recipes/1/favorite", [this](const httplib::Request&, httplib::Response& res) {
            favoriteWriteReqCount++;
            res.status = favoriteWriteStatus.load();
            res.set_content(R"({"message":"ok"})", "application/json");
        });

        port = svr.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("FavoriteStubServer: bind_to_any_port failed");
        th = std::thread([this]() { svr.listen_after_bind(); });
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
} // namespace
