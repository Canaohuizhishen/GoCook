// test_avatar_files.cpp —— AvatarFiles（头像文件生命周期原语）单元测试
//
// 覆盖：
//   · isManagedAvatarUrl：命名空间前缀、文件名单段、用户前缀严格边界（user_1_ vs user_10_）
//   · fileExists：常规文件判定；目录/不存在 → false
//   · removeFile：删除成功、幂等（已不存在仍成功）、拒绝非托管路径、目录不被删除
//
// 不依赖数据库：GOCOOK_UPLOADS_DIR 经 EnvGuard 指向用例自建临时目录，
// 作用域结束恢复原值（与 test_upload_paths.cpp 同一模式）。

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "../common/AvatarFiles.h"

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

private:
    std::string name_;
    std::optional<std::string> saved_;
};

// 每个用例自建独立上传根目录（<tmp>/gocook_avatar_files_<用例名>/avatars/）
class AvatarFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        base_ = std::filesystem::temp_directory_path()
              / (std::string("gocook_avatar_files_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
        std::filesystem::create_directories(base_ / "avatars");
        guard_.emplace(kUploadsDirEnv);
        guard_->set(base_.c_str());
    }

    void TearDown() override {
        guard_.reset();  // 先恢复环境变量
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    /// 在 avatars/ 下写一个测试文件
    void writeAvatar(const std::string& filename) {
        std::ofstream ofs(base_ / "avatars" / filename, std::ios::binary);
        ofs << "test-content";
    }

    std::filesystem::path base_;
    std::optional<EnvGuard> guard_;
};

} // namespace

// ==================== 归属判定 ====================

TEST_F(AvatarFilesTest, 归属判定_本人托管url通过) {
    EXPECT_TRUE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_1_1234567890.jpg", 1));
    EXPECT_TRUE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_42_1.png"));
    EXPECT_TRUE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_42_1.png", 42));
}

TEST_F(AvatarFilesTest, 归属判定_用户前缀严格边界匹配) {
    // "user_1_" 不得误配 "user_10_..."（短前缀陷阱）
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_10_123.jpg", 1));
    EXPECT_TRUE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_10_123.jpg", 10));
    // 恰好等于用户前缀、没有文件名主体 → 拒绝
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/user_1_", 1));
}

TEST_F(AvatarFilesTest, 归属判定_穿越与他人路径拒绝) {
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/../user_1_1.jpg", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/sub/user_1_1.jpg", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/recipes/user_1_1.jpg", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatars/", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("/uploads/avatarsX/user_1_1.jpg", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("https://x.com/uploads/avatars/user_1_1.jpg", 1));
    EXPECT_FALSE(AvatarFiles::isManagedAvatarUrl("", 1));
}

// ==================== 存在性 ====================

TEST_F(AvatarFilesTest, 存在性_常规文件为真目录为假) {
    writeAvatar("user_1_100.jpg");
    EXPECT_TRUE(AvatarFiles::fileExists("/uploads/avatars/user_1_100.jpg"));
    EXPECT_FALSE(AvatarFiles::fileExists("/uploads/avatars/user_1_999.jpg"));  // 不存在
    std::filesystem::create_directories(base_ / "avatars" / "user_1_200.jpg");
    EXPECT_FALSE(AvatarFiles::fileExists("/uploads/avatars/user_1_200.jpg"));  // 目录
}

// ==================== 回收 ====================

TEST_F(AvatarFilesTest, 回收_删除成功且幂等) {
    writeAvatar("user_1_100.jpg");
    EXPECT_TRUE(AvatarFiles::removeFile("/uploads/avatars/user_1_100.jpg"));
    EXPECT_FALSE(AvatarFiles::fileExists("/uploads/avatars/user_1_100.jpg"));
    // 幂等：文件已不存在仍返回成功，可安全重试
    EXPECT_TRUE(AvatarFiles::removeFile("/uploads/avatars/user_1_100.jpg"));
}

TEST_F(AvatarFilesTest, 回收_非托管路径拒绝) {
    EXPECT_FALSE(AvatarFiles::removeFile("/etc/passwd"));
    EXPECT_FALSE(AvatarFiles::removeFile("/uploads/recipes/user_1_1.jpg"));
    EXPECT_FALSE(AvatarFiles::removeFile(""));
}

TEST_F(AvatarFilesTest, 回收_目录不被删除) {
    std::filesystem::create_directories(base_ / "avatars" / "user_1_1.jpg");
    EXPECT_FALSE(AvatarFiles::removeFile("/uploads/avatars/user_1_1.jpg"));
    EXPECT_TRUE(std::filesystem::exists(base_ / "avatars" / "user_1_1.jpg"));
}
