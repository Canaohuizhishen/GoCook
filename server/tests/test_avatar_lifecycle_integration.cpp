// test_avatar_lifecycle_integration.cpp —— 头像文件生命周期（PgUserRepository）集成测试（依赖真实 PostgreSQL）
//
// 锁定"上传暂存 → 保存绑定 → 回收"的文件与数据库语义（mock 测试测不到真实文件系统行为）：
//   · uploadAvatar：仅暂存（落盘生成新文件且不改 users.avatar_url），临时源文件被清理
//   · updateProfile：换绑后旧头像文件被回收；绑定值与旧值相同则不删
//   · deleteAccount：注销后头像文件被回收
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。上传目录：GOCOOK_UPLOADS_DIR 指向用例自建临时目录
// （EnvGuard 作用域恢复）。测试数据使用"集成测试"前缀用户，SetUp/TearDown 清理。
// （模式同 test_favorites_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "../common/AvatarFiles.h"
#include "../common/ConnectionPool.h"
#include "../common/UploadPaths.h"
#include "../repositories/PgUserRepository.h"
#include <gocook/DataModels.h>
#include <gocook/IServices.h>

namespace {

const char* kDefaultTestConn =
    "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";

std::string testConnString() {
    const char* env = std::getenv("GOCOOK_TEST_DB");
    return env ? std::string(env) : std::string(kDefaultTestConn);
}

ConnectionPool& testPool() {
    static ConnectionPool pool(testConnString(), /*maxSize=*/1);
    return pool;
}

bool dbAvailable() {
    try {
        auto guard = testPool().getConnection(std::chrono::milliseconds(2000));
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("SELECT 1");
        return true;
    } catch (...) {
        return false;
    }
}

// 环境变量守卫：用例内 set，析构时恢复原值（与 test_upload_paths.cpp 同一模式）
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

constexpr const char* kTestUsername = "集成测试头像用户";

} // namespace

class AvatarLifecycleDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        dbOk_ = true;

        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        base_ = std::filesystem::temp_directory_path()
              / (std::string("gocook_avatar_it_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
        std::filesystem::create_directories(base_ / "avatars");

        guard_.emplace("GOCOOK_UPLOADS_DIR");
        guard_->set(base_.c_str());

        cleanup();
    }

    void TearDown() override {
        if (!dbOk_) return;
        cleanup();
        guard_.reset();
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    static void cleanup() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("DELETE FROM users WHERE username = $1", pqxx::params{kTestUsername});
    }

    /// 创建（或复用）测试用户，返回用户 id
    static int ensureUser() {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, 'x')"
            " ON CONFLICT (username) DO UPDATE SET username = EXCLUDED.username"
            " RETURNING id",
            pqxx::params{kTestUsername});
        return r[0][0].as<int>();
    }

    static void setAvatarUrl(int userId, const std::string& url) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        ntxn.exec("UPDATE users SET avatar_url = $1 WHERE id = $2",
                  pqxx::params{url, userId});
    }

    static std::string getAvatarUrl(int userId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec("SELECT avatar_url FROM users WHERE id = $1",
                                   pqxx::params{userId});
        return r.empty() ? std::string() : r[0]["avatar_url"].as<std::string>("");
    }

    void writeAvatarFile(const std::string& filename) {
        std::ofstream ofs(base_ / "avatars" / filename, std::ios::binary);
        ofs << "x";
    }

    bool avatarFileExists(const std::string& filename) {
        return std::filesystem::exists(base_ / "avatars" / filename);
    }

    /// 该用户命名空间下的托管文件名（如 user_<id>_1610000001.jpg）
    static std::string avatarName(int userId, long ts) {
        return "user_" + std::to_string(userId) + "_" + std::to_string(ts) + ".jpg";
    }

    std::filesystem::path base_;
    std::optional<EnvGuard> guard_;
    bool dbOk_ = false;
};

TEST_F(AvatarLifecycleDbTest, 上传仅暂存不改资料且清临时文件) {
    const int userId = ensureUser();
    const std::string oldName = avatarName(userId, 1610000000);
    writeAvatarFile(oldName);
    setAvatarUrl(userId, "/uploads/avatars/" + oldName);

    // 临时源文件（模拟 handler 写出的 /tmp 暂存文件）
    auto src = std::filesystem::temp_directory_path() / "gocook_avatar_it_src.jpg";
    {
        std::ofstream ofs(src, std::ios::binary);
        ofs << "new-image";
    }

    PgUserRepository repo(testPool());
    auto resp = repo.uploadAvatar(userId, src.string());

    // 新文件已落盘；返回暂存引用；资料（avatar_url）与旧文件都未被触碰
    EXPECT_TRUE(AvatarFiles::isManagedAvatarUrl(resp.avatar_url, userId));
    EXPECT_TRUE(std::filesystem::exists(UploadPaths::urlToPath(resp.avatar_url, "avatars")));
    EXPECT_EQ(getAvatarUrl(userId), "/uploads/avatars/" + oldName);
    EXPECT_TRUE(avatarFileExists(oldName));
    // 临时源文件已清理
    EXPECT_FALSE(std::filesystem::exists(src));
}

TEST_F(AvatarLifecycleDbTest, 换绑回收被顶替文件) {
    const int userId = ensureUser();
    const std::string nameA = avatarName(userId, 1610000001);
    const std::string nameB = avatarName(userId, 1610000002);
    writeAvatarFile(nameA);
    writeAvatarFile(nameB);
    setAvatarUrl(userId, "/uploads/avatars/" + nameA);

    PgUserRepository repo(testPool());
    gocook::models::UpdateProfileRequest profile;
    profile.avatar_url = "/uploads/avatars/" + nameB;
    repo.updateProfile(userId, profile);

    EXPECT_EQ(getAvatarUrl(userId), "/uploads/avatars/" + nameB);
    EXPECT_FALSE(avatarFileExists(nameA));  // 被顶替的旧文件已回收
    EXPECT_TRUE(avatarFileExists(nameB));   // 新绑定文件保留
}

TEST_F(AvatarLifecycleDbTest, 绑定值与旧值相同不删文件) {
    const int userId = ensureUser();
    const std::string name = avatarName(userId, 1610000003);
    writeAvatarFile(name);
    setAvatarUrl(userId, "/uploads/avatars/" + name);

    PgUserRepository repo(testPool());
    gocook::models::UpdateProfileRequest profile;
    profile.avatar_url = "/uploads/avatars/" + name;  // 相同值：不应误删
    repo.updateProfile(userId, profile);

    EXPECT_TRUE(avatarFileExists(name));
}

TEST_F(AvatarLifecycleDbTest, 注销回收头像文件) {
    const int userId = ensureUser();
    const std::string name = avatarName(userId, 1610000004);
    writeAvatarFile(name);
    setAvatarUrl(userId, "/uploads/avatars/" + name);

    PgUserRepository repo(testPool());
    repo.deleteAccount(userId);

    EXPECT_FALSE(avatarFileExists(name));  // 注销后头像文件已回收
    EXPECT_EQ(getAvatarUrl(userId), "");   // 用户行已删除
}
