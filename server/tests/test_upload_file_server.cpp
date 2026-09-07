// test_upload_file_server.cpp —— UploadFileServer（上传文件读取服务）集成测试
//
// 覆盖：
//   · 正常服务：200 + 正确 Content-Type + 字节一致
//   · 大写扩展名回归：.PNG 必须按小写匹配 MIME（旧头像实现会把 .PNG 当 jpeg）
//   · 第一层穿越防御：URL 含 .. 或 / → 400
//   · 第二层穿越防御：目录内符号链接指向目录外 → 400；指向同前缀兄弟目录
//     （如 avatars_evil）→ 400（组件边界判断；环境不支持建链接则跳过）
//   · 不依赖数据库：GOCOOK_UPLOADS_DIR 指向用例自建临时目录；用 httplib
//   bind_to_any_port + 后台线程起真实 HTTP 服务，再以 httplib::Client 请求。

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <unistd.h>

#include <httplib/httplib.h>

#include "../common/ImageUploadRules.h"
#include "../common/UploadFileServer.h"

namespace {

constexpr const char* kUploadsDirEnv = "GOCOOK_UPLOADS_DIR";

// 环境变量守卫：用例内 set/clear 修改，析构时恢复原值（原值不存在则清除）
class EnvGuard {
public:
    explicit EnvGuard(const char* name) : name_(name) {
        if (const char* v = std::getenv(name); v != nullptr)
            saved_ = std::string(v);
    }
    ~EnvGuard() {
        if (saved_)
            setenv(name_.c_str(), saved_->c_str(), 1);
        else
            unsetenv(name_.c_str());
    }

    void set(const char* value) { setenv(name_.c_str(), value, 1); }
    void clear() { unsetenv(name_.c_str()); }

private:
    std::string name_;
    std::optional<std::string> saved_;
};

// 为 subdir 注册路由并起真实 HTTP 监听（临时端口），析构时停止
class TestFileServer {
public:
    ~TestFileServer() {
        if (svr_.is_running())
            svr_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    // @param payloadCap HTTP 层请求体上限（0 = 不设，httplib 默认无上限）
    // @return 监听是否就绪
    bool start(const std::string& subdir, const std::string& label = {},
               size_t payloadCap = 0) {
        if (payloadCap > 0)
            svr_.set_payload_max_length(payloadCap);
        UploadFileServer::registerUploadRoutes(svr_, subdir, label);
        port_ = svr_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0)
            return false;
        thread_ = std::thread([this] { svr_.listen_after_bind(); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!svr_.is_running() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        return svr_.is_running();
    }

    httplib::Client client() const { return httplib::Client("127.0.0.1", port_); }

private:
    httplib::Server svr_;
    int port_ = -1;
    std::thread thread_;
};

// 常见图片文件的魔数片段（服务端按字节原样返回，不解析内容，此处仅作数据）
const std::string kJpegBytes = "\xFF\xD8\xFF\xE0\x00\x10JFIF...fake-jpeg";
const std::string kPngBytes  = "\x89PNG\r\n\x1a\n...fake-png";

class UploadFileServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        static int counter = 0;
        base_ = std::filesystem::temp_directory_path()
              / ("gocook_ufs_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
        std::filesystem::remove_all(base_);
        std::filesystem::create_directories(base_);
        env_.set(base_.c_str());
    }

    void TearDown() override {
        std::filesystem::remove_all(base_);
    }

    // 在上传根目录 <subdir>/ 下写入 <name> 文件
    void createUploadFile(const std::string& subdir, const std::string& name,
                          const std::string& data) {
        const auto dir = base_ / subdir;
        std::filesystem::create_directories(dir);
        std::ofstream ofs(dir / name, std::ios::binary);
        ofs << data;
    }

    std::filesystem::path base_;
    EnvGuard env_{kUploadsDirEnv};
};

TEST_F(UploadFileServerTest, 正常返回文件内容与正确MIME) {
    createUploadFile("avatars", "photo.jpg", kJpegBytes);
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/photo.jpg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(200, res->status);
    EXPECT_EQ("image/jpeg", res->get_header_value("Content-Type"));
    EXPECT_EQ(kJpegBytes, res->body);
}

TEST_F(UploadFileServerTest, 大写扩展名按小写匹配MIME回归) {
    // 旧头像实现不转小写，.PNG 会被误判为 image/jpeg —— 回归保护
    createUploadFile("avatars", "PIC.PNG", kPngBytes);
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/PIC.PNG");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(200, res->status);
    EXPECT_EQ("image/png", res->get_header_value("Content-Type"));
    EXPECT_EQ(kPngBytes, res->body);
}

TEST_F(UploadFileServerTest, 路径穿越含点号或斜杠返回400) {
    createUploadFile("avatars", "secret.jpg", kJpegBytes);
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/../secret.jpg");  // 含 '/'
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(400, res->status);

    auto res2 = srv.client().Get("/uploads/avatars/..");            // 含 ".."
    ASSERT_TRUE(res2 != nullptr);
    EXPECT_EQ(400, res2->status);
}

TEST_F(UploadFileServerTest, 符号链接指向目录外被第二层防御拦截) {
    createUploadFile("avatars", "real.jpg", kJpegBytes);
    // 目录外放一个"秘密"文件，并在 avatars 内建指向它的符号链接
    {
        std::ofstream ofs(base_ / "outside_secret.jpg", std::ios::binary);
        ofs << "top-secret";
    }
    try {
        std::filesystem::create_symlink(base_ / "outside_secret.jpg",
                                        base_ / "avatars" / "link.jpg");
    } catch (const std::exception&) {
        GTEST_SKIP() << "当前环境不支持创建符号链接，跳过第二层防御用例";
    }

    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/link.jpg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(400, res->status);  // 链接逃逸允许目录 → 拦截
}

TEST_F(UploadFileServerTest, 符号链接指向同前缀兄弟目录被组件边界拦截) {
    // 兄弟目录与本目录同名前缀（avatars vs avatars_evil）：纯字符串前缀判断会
    // 放行，需要组件级边界判断拦截（修复前此用例返回 200 造成泄露）
    createUploadFile("avatars", "real.jpg", kJpegBytes);
    createUploadFile("avatars_evil", "secret.jpg", "top-secret-prefix-leak");
    try {
        std::filesystem::create_symlink(base_ / "avatars_evil" / "secret.jpg",
                                        base_ / "avatars" / "link.jpg");
    } catch (const std::exception&) {
        GTEST_SKIP() << "当前环境不支持创建符号链接，跳过第二层防御用例";
    }

    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/link.jpg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(400, res->status);  // 链接逃逸到同前缀兄弟目录 → 组件边界拦截
}

TEST_F(UploadFileServerTest, 不存在的文件返回404) {
    createUploadFile("avatars", "photo.jpg", kJpegBytes);
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/missing.png");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(404, res->status);
}

TEST_F(UploadFileServerTest, 目录不存在时注册路由自动创建) {
    TestFileServer srv;
    ASSERT_TRUE(srv.start("auto_mkdir", "自动建目录"));

    EXPECT_TRUE(std::filesystem::exists(base_ / "auto_mkdir"));

    auto res = srv.client().Get("/uploads/auto_mkdir/nothing.png");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(404, res->status);
}

TEST_F(UploadFileServerTest, 响应携带CSP与nosniff头) {
    createUploadFile("avatars", "photo.jpg", kJpegBytes);
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/photo.jpg");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(200, res->status);
    // svg 以顶级文档打开时可能含脚本（写侧黑名单不承诺完备 + 历史存量文件未过滤）：
    // CSP sandbox 是读侧权威防线；作为 <img> 或 Qt 客户端加载时浏览器忽略 CSP
    EXPECT_EQ("sandbox; default-src 'none'; style-src 'unsafe-inline'",
              res->get_header_value("Content-Security-Policy"));
    EXPECT_EQ("nosniff", res->get_header_value("X-Content-Type-Options"));

    // 错误响应同样携带（头在 handler 顶部统一设置，覆盖全部返回路径）
    auto miss = srv.client().Get("/uploads/avatars/missing.png");
    ASSERT_TRUE(miss != nullptr);
    EXPECT_EQ(404, miss->status);
    EXPECT_FALSE(miss->get_header_value("Content-Security-Policy").empty());
    EXPECT_FALSE(miss->get_header_value("X-Content-Type-Options").empty());
}

TEST_F(UploadFileServerTest, 子目录等非常规文件请求返回404) {
    // 目录名恰好等于请求文件名时，旧实现 exists 为真 → ifstream 打开目录读出
    // 空 body 却返回 200 空内容；现在非常规文件一律 404
    std::filesystem::create_directories(base_ / "avatars" / "subdir");

    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars"));

    auto res = srv.client().Get("/uploads/avatars/subdir");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(404, res->status);
    EXPECT_EQ("未找到", res->body);
}

TEST_F(UploadFileServerTest, HTTP层请求体上限超5MB返回413恰5MB放行) {
    // Router::setupRoutes 对生产服务器设 set_payload_max_length(kMaxImageBytes)：
    // httplib 在路由前先读完整 body，handler 内 5MB 判断挡不住超大 Content-Length
    // 请求的内存占用，上限必须在 HTTP 读取层。此处用同一常量验证边界语义：
    // 恰 5MB 不被 HTTP 层拒（本服务只有 GET 路由，POST 落到路由层回 404 属预期）；
    // 超 1 字节在读取层直接 413（body 被跳过，不缓冲）。
    TestFileServer srv;
    ASSERT_TRUE(srv.start("avatars", "头像", ImageUploadRules::kMaxImageBytes));

    std::string atLimit(ImageUploadRules::kMaxImageBytes, 'A');
    auto res1 = srv.client().Post("/uploads/avatars/x.png", atLimit, "image/png");
    ASSERT_TRUE(res1 != nullptr);
    EXPECT_NE(413, res1->status);

    std::string tooBig(ImageUploadRules::kMaxImageBytes + 1, 'A');
    auto res2 = srv.client().Post("/uploads/avatars/x.png", tooBig, "image/png");
    ASSERT_TRUE(res2 != nullptr);
    EXPECT_EQ(413, res2->status);
}

} // namespace
