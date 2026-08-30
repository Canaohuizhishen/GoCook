// test_upload_paths.cpp —— UploadPaths（上传路径规则收口）单元测试
//
// 覆盖：
//   · baseDir()：默认 "server/uploads" 转绝对路径；GOCOOK_UPLOADS_DIR 环境变量覆盖
//   · urlToPath()：存储 URL → 磁盘真实路径；无 '/' 的 URL / 空串 → 空（调用方跳过）
//   · 边界：外部完整 URL 只取 basename、环境变量带尾斜杠（双斜杠拼接可接受）
//
// 不依赖数据库与真实文件系统，纯字符串/路径语义断言。
// 环境变量通过 EnvGuard 在用例内修改、作用域结束恢复，不影响其他测试。

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "../common/UploadPaths.h"

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

} // namespace

TEST(UploadPathsTest, 默认根目录为server_uploads的绝对路径) {
    EnvGuard guard(kUploadsDirEnv);
    guard.clear();
    EXPECT_EQ(UploadPaths::baseDir(),
              std::filesystem::absolute("server/uploads").string());
}

TEST(UploadPathsTest, 环境变量覆盖上传根目录) {
    EnvGuard guard(kUploadsDirEnv);
    guard.set("/tmp/gocook_test_uploads");
    EXPECT_EQ(UploadPaths::baseDir(), "/tmp/gocook_test_uploads");
    EXPECT_EQ(UploadPaths::urlToPath("/uploads/recipes/a.jpg", "recipes"),
              "/tmp/gocook_test_uploads/recipes/a.jpg");
}

TEST(UploadPathsTest, urlToPath取URL文件名拼接到子目录) {
    EnvGuard guard(kUploadsDirEnv);
    guard.set("/data/uploads");
    EXPECT_EQ(UploadPaths::urlToPath("/uploads/recipes/tomato-egg.jpg", "recipes"),
              "/data/uploads/recipes/tomato-egg.jpg");
    EXPECT_EQ(UploadPaths::urlToPath("/uploads/avatars/user_1_123.png", "avatars"),
              "/data/uploads/avatars/user_1_123.png");
}

TEST(UploadPathsTest, urlToPath无斜杠或空串返回空) {
    EnvGuard guard(kUploadsDirEnv);
    guard.set("/data/uploads");
    EXPECT_EQ(UploadPaths::urlToPath("a.jpg", "recipes"), "");
    EXPECT_EQ(UploadPaths::urlToPath("", "recipes"), "");
}

TEST(UploadPathsTest, urlToPath外部完整URL只取basename) {
    EnvGuard guard(kUploadsDirEnv);
    guard.set("/data/uploads");
    // 存储 URL 约定为 /uploads/<subdir>/<file>；外部完整 URL 同样只取文件名
    EXPECT_EQ(UploadPaths::urlToPath("https://cdn.example.com/img/recipe-1.jpg", "recipes"),
              "/data/uploads/recipes/recipe-1.jpg");
}

TEST(UploadPathsTest, 环境变量带尾斜杠双斜杠拼接可接受) {
    EnvGuard guard(kUploadsDirEnv);
    guard.set("/data/uploads/");
    // filesystem::absolute 保留尾斜杠，拼接产生 //，对文件系统操作无害（与旧实现行为一致）
    EXPECT_EQ(UploadPaths::urlToPath("/uploads/recipes/a.jpg", "recipes"),
              "/data/uploads//recipes/a.jpg");
}
