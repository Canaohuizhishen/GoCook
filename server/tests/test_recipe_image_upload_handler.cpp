// test_recipe_image_upload_handler.cpp —— 菜谱封面/步骤图上传（Handler 层）测试
//
// 覆盖（httplib 真实路由 + 自签 JWT + 真 RecipeServiceImpl + Mock 仓库）：
//   · 鉴权：无令牌 / 坏令牌 → 401「无效的访问令牌」
//   · 空体（两接口）→ 400「请选择图片」（v2.22 统一文案）
//   · 声明 webp（白名单外）→ 400 规范文案（含 WebP 注记）
//   · 声明合法但内容不可识别 → 400「图片内容与格式不符，请重新选择」
//   · 超限（>5MB）→ 400「图片大小不能超过5MB」（本测试服务器不设 HTTP 层上限，
//     直测 Handler 分支；生产 Router 的 5MB 读取层上限见 test_upload_file_server）
//   · 封面成功 → 200 + image_url；userId/recipeId 已透传；临时文件已写出（前缀校验）
//   · 步骤图成功 → 200 + image_url + message；recipeId/stepIndex 解析正确
//   · 仓库拒绝 → 403/404 异常映射（ErrorHelper 语义：403 透传文案、404 统一文案）
//
// 临时文件说明：成功路径的临时文件由真实仓库负责消费，此处 Mock 不消费，
// 用例在断言后自行删除（/tmp/gocook_recipe_*），防残留。

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <httplib/httplib.h>
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "../common/ImageUploadRules.h"
#include "../handlers/RecipeHandler.h"
#include "../middleware/auth_middleware.h"
#include "../services/RecipeServiceImpl.h"
#include "MockRecipeRepository.h"

using ::testing::_;
using ::testing::NiceMock;
using gocook::services::ServiceException;

namespace {

constexpr const char* kTestSecret = "recipe-image-handler-test-secret";
constexpr int kUserId = 7;

/// 自签 JWT：载荷结构与 UserServiceImpl::generateToken 一致（iss=GoCook／userId／username／role／exp）
std::string mintToken(int userId) {
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer("GoCook")
        .set_type("JWS")
        .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
        .set_payload_claim("username", jwt::claim(std::string("tester")))
        .set_payload_claim("role", jwt::claim(std::string("user")))
        .set_issued_at(now)
        .set_expires_at(now + std::chrono::hours(1))
        .sign(jwt::algorithm::hs256{kTestSecret});
}

// 真实图片魔数（Handler 按魔数宽容修正，内容仅需可识别）
const std::string kJpegBytes = "\xFF\xD8\xFF\xE0\x00\x10JFIF-fake";
const std::string kPngBytes = std::string("\x89PNG\r\n\x1a\n", 8) + "fake-png";

// 与 Router 注册完全一致的业务路由（正则含捕获组，直测 Handler 的 matches 解析）
class HandlerTestServer {
public:
    explicit HandlerTestServer(RecipeHandler& handler) {
        svr_.Post(R"(/api/recipes/(\d+)/image)",
                  [&handler](const httplib::Request& req, httplib::Response& res) {
                      handler.uploadRecipeImage(req, res);
                  });
        svr_.Post(R"(/api/recipes/(\d+)/steps/(\d+)/image)",
                  [&handler](const httplib::Request& req, httplib::Response& res) {
                      handler.uploadStepImage(req, res);
                  });
        port_ = svr_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0)
            throw std::runtime_error("HandlerTestServer: bind_to_any_port failed");
        thread_ = std::thread([this] { svr_.listen_after_bind(); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!svr_.is_running() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!svr_.is_running())
            throw std::runtime_error("HandlerTestServer: failed to start");
    }

    ~HandlerTestServer() {
        svr_.stop();
        if (thread_.joinable()) thread_.join();
    }

    httplib::Client client() const { return httplib::Client("127.0.0.1", port_); }

private:
    httplib::Server svr_;
    int port_ = 0;
    std::thread thread_;
};

class RecipeImageHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
        repo_ = mock.get();
        service_ = std::make_unique<RecipeServiceImpl>(std::move(mock));
        auth_ = std::make_unique<AuthMiddleware>(kTestSecret);
        handler_ = std::make_unique<RecipeHandler>(*service_, *auth_);
        // 最后构造 → 最先析构：先停 HTTP 服务（其在途请求持有 handler 引用）再拆依赖
        srv_ = std::make_unique<HandlerTestServer>(*handler_);
        token_ = "Bearer " + mintToken(kUserId);
    }

    httplib::Headers authHeaders() const { return {{"Authorization", token_}}; }

    NiceMock<MockRecipeRepository>* repo_ = nullptr;
    std::unique_ptr<RecipeServiceImpl> service_;
    std::unique_ptr<AuthMiddleware> auth_;
    std::unique_ptr<RecipeHandler> handler_;
    std::unique_ptr<HandlerTestServer> srv_;
    std::string token_;
};

} // namespace

// ==================== 鉴权 ====================

TEST_F(RecipeImageHandlerTest, 未认证与坏令牌返回401) {
    auto noAuth = srv_->client().Post("/api/recipes/42/image", kJpegBytes, "image/jpeg");
    ASSERT_TRUE(noAuth != nullptr);
    EXPECT_EQ(401, noAuth->status);
    EXPECT_THAT(noAuth->body, testing::HasSubstr("无效的访问令牌"));

    auto badAuth = srv_->client().Post("/api/recipes/42/image",
        httplib::Headers{{"Authorization", "Bearer not-a-jwt"}}, kJpegBytes, "image/jpeg");
    ASSERT_TRUE(badAuth != nullptr);
    EXPECT_EQ(401, badAuth->status);
}

// ==================== 分类失败：统一文案（v2.22 收口） ====================

TEST_F(RecipeImageHandlerTest, 空体与分类失败返回统一文案) {
    // 空体（两接口）→「请选择图片」
    auto emptyCover = srv_->client().Post("/api/recipes/1/image", authHeaders(), "", "image/png");
    ASSERT_TRUE(emptyCover != nullptr);
    EXPECT_EQ(400, emptyCover->status);
    EXPECT_THAT(emptyCover->body, testing::HasSubstr("请选择图片"));

    auto emptyStep = srv_->client().Post("/api/recipes/1/steps/0/image", authHeaders(), "", "image/png");
    ASSERT_TRUE(emptyStep != nullptr);
    EXPECT_EQ(400, emptyStep->status);
    EXPECT_THAT(emptyStep->body, testing::HasSubstr("请选择图片"));

    // 声明 webp（白名单外）→ 规范文案（含 WebP 注记，与头像/契约一致）
    auto webp = srv_->client().Post("/api/recipes/1/image", authHeaders(), kJpegBytes, "image/webp");
    ASSERT_TRUE(webp != nullptr);
    EXPECT_EQ(400, webp->status);
    EXPECT_THAT(webp->body,
        testing::HasSubstr("不支持的图片格式，请使用 JPG/PNG/GIF/BMP/SVG（注：Qt 客户端不支持 WebP）"));

    // 伪装格式（声明合法、内容不可识别）→ 内容不符文案
    auto bad = srv_->client().Post("/api/recipes/1/image", authHeaders(), "not-an-image", "image/png");
    ASSERT_TRUE(bad != nullptr);
    EXPECT_EQ(400, bad->status);
    EXPECT_THAT(bad->body, testing::HasSubstr("图片内容与格式不符，请重新选择"));
}

TEST_F(RecipeImageHandlerTest, 超限返回5MB文案) {
    const std::string tooBig(ImageUploadRules::kMaxImageBytes + 1, 'A');
    auto res = srv_->client().Post("/api/recipes/1/image", authHeaders(), tooBig, "image/png");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(400, res->status);
    EXPECT_THAT(res->body, testing::HasSubstr("图片大小不能超过5MB"));
}

// ==================== 成功路径：委派参数与临时文件 ====================

TEST_F(RecipeImageHandlerTest, 封面成功透传用户与菜谱并返回URL) {
    std::string capturedPath;
    EXPECT_CALL(*repo_, updateRecipeImage(kUserId, 42, _))
        .WillOnce([&](int, int, const std::string& p) {
            capturedPath = p;
            return std::string("/uploads/recipes/recipe_42_123.jpg");
        });

    auto res = srv_->client().Post("/api/recipes/42/image", authHeaders(), kJpegBytes, "image/jpeg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(200, res->status);
    const auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ("/uploads/recipes/recipe_42_123.jpg", body["image_url"].get<std::string>());

    // 临时文件：本次请求已写出（Mock 仓库不消费 → 用例自查后清理）
    EXPECT_THAT(capturedPath, testing::HasSubstr("/tmp/gocook_recipe_42_"));
    EXPECT_TRUE(std::filesystem::exists(capturedPath));
    std::filesystem::remove(capturedPath);
}

TEST_F(RecipeImageHandlerTest, 步骤图成功解析索引并返回URL与消息) {
    std::string capturedPath;
    EXPECT_CALL(*repo_, updateStepImage(kUserId, 42, 2, _))
        .WillOnce([&](int, int, int, const std::string& p) {
            capturedPath = p;
            return std::string("/uploads/recipes/recipe_42_step_2_123.jpg");
        });

    auto res = srv_->client().Post("/api/recipes/42/steps/2/image", authHeaders(), kPngBytes, "image/png");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(200, res->status);
    const auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ("/uploads/recipes/recipe_42_step_2_123.jpg", body["image_url"].get<std::string>());
    EXPECT_EQ("步骤图已上传", body["message"].get<std::string>());

    EXPECT_THAT(capturedPath, testing::HasSubstr("gocook_recipe_42_step_2_"));
    EXPECT_TRUE(std::filesystem::exists(capturedPath));
    std::filesystem::remove(capturedPath);
}

// ==================== 仓库拒绝的异常映射（越权 403 / 不存在 404） ====================

TEST_F(RecipeImageHandlerTest, 仓库拒绝映射为403与404) {
    std::string forbiddenPath;
    EXPECT_CALL(*repo_, updateRecipeImage(kUserId, 42, _))
        .WillOnce([&](int, int, const std::string& p) -> std::string {
            forbiddenPath = p;
            throw ServiceException("仅可编辑自己投稿的菜谱", 403);
        });
    auto res = srv_->client().Post("/api/recipes/42/image", authHeaders(), kJpegBytes, "image/jpeg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(403, res->status);
    EXPECT_THAT(res->body, testing::HasSubstr("仅可编辑自己投稿的菜谱"));

    std::string missingPath;
    EXPECT_CALL(*repo_, updateStepImage(kUserId, 99, 0, _))
        .WillOnce([&](int, int, int, const std::string& p) -> std::string {
            missingPath = p;
            throw ServiceException("菜谱不存在", 404);
        });
    auto res2 = srv_->client().Post("/api/recipes/99/steps/0/image", authHeaders(), kJpegBytes, "image/jpeg");
    ASSERT_TRUE(res2 != nullptr);
    EXPECT_EQ(404, res2->status);
    EXPECT_THAT(res2->body, testing::HasSubstr("请求的资源不存在"));  // ErrorHelper 404 统一文案

    // 生产链路中临时文件由仓库层清理（Mock 不清理）：用例负责收尾，防残留
    std::error_code ec;
    if (!forbiddenPath.empty()) std::filesystem::remove(forbiddenPath, ec);
    if (!missingPath.empty()) std::filesystem::remove(missingPath, ec);
}
