// test_recipe_image_lifecycle_integration.cpp —— 菜谱封面/步骤图文件生命周期（PgRecipeRepository）集成测试（依赖真实 PostgreSQL）
//
// 锁定"上传落盘 → DB 更新 → 失败清理"的文件与数据库语义（Mock 测试测不到真实文件系统行为）：
//   · updateRecipeImage：新文件落盘 uploads/recipes + image_url 入 DB + 临时源文件清理
//   · updateStepImage：对应步骤 image_url 写入 steps JSONB + 落盘 + 临时源文件清理
//   · 越权（非作者）403 / 菜谱不存在 404 / 步骤索引越界 400：不产生任何文件残留
//     （临时源文件被清理；uploads/recipes 下无按本菜谱命名的目标文件）
//
// 连接串：默认本地开发库（与 .env / docker-compose.yml 一致），可用环境变量 GOCOOK_TEST_DB
// 覆盖；探测失败即跳过（GTEST_SKIP）。上传目录：GOCOOK_UPLOADS_DIR 指向用例自建临时目录
// （EnvGuard 作用域恢复）。测试数据使用"集成测试"前缀用户/菜谱，SetUp/TearDown 清理。
// （模式同 test_avatar_lifecycle_integration.cpp）

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "../common/ConnectionPool.h"
#include "../common/UploadPaths.h"
#include "../repositories/PgRecipeRepository.h"
#include <gocook/IServices.h>

using gocook::services::ServiceException;

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

constexpr const char* kTestUsername = "集成测试菜谱图用户";

} // namespace

class RecipeImageLifecycleDbTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!dbAvailable())
            GTEST_SKIP() << "未检测到可用 PostgreSQL（可用 GOCOOK_TEST_DB 指定连接串）";
        dbOk_ = true;

        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        base_ = std::filesystem::temp_directory_path()
              / (std::string("gocook_recipe_it_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
        std::filesystem::create_directories(base_ / "recipes");

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
        // author_id 无 CASCADE：先删菜谱再删用户
        ntxn.exec("DELETE FROM recipes WHERE author_id IN"
                  " (SELECT id FROM users WHERE username = $1)",
                  pqxx::params{kTestUsername});
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

    /// 建测试菜谱（steps 给定 JSON），返回菜谱 id
    static int createRecipe(int authorId, const std::string& stepsJson) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec(
            "INSERT INTO recipes (name, author_id, steps)"
            " VALUES ('集成测试菜谱图', $1, $2::jsonb) RETURNING id",
            pqxx::params{authorId, stepsJson});
        return r[0][0].as<int>();
    }

    static std::string getImageUrl(int recipeId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec("SELECT image_url FROM recipes WHERE id = $1",
                                   pqxx::params{recipeId});
        return r.empty() ? std::string() : r[0]["image_url"].as<std::string>("");
    }

    static std::string getStepsJson(int recipeId) {
        auto guard = testPool().getConnection();
        pqxx::nontransaction ntxn(*guard);
        pqxx::result r = ntxn.exec("SELECT steps FROM recipes WHERE id = $1",
                                   pqxx::params{recipeId});
        return r.empty() ? std::string("[]") : r[0]["steps"].as<std::string>("[]");
    }

    /// 写一个临时源文件（模拟 handler 写出的 /tmp 文件），返回其路径
    std::string writeTempSource(const std::string& tag) {
        const auto p = std::filesystem::temp_directory_path()
                     / ("gocook_recipe_it_src_" + tag + ".jpg");
        std::ofstream ofs(p, std::ios::binary);
        ofs << "fake-image";
        return p.string();
    }

    /// uploads/recipes 下以 prefix 开头的文件数（断言失败路径无新文件残留）
    size_t countUploadsWithPrefix(const std::string& prefix) const {
        size_t n = 0;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(base_ / "recipes", ec)) {
            if (e.path().filename().string().rfind(prefix, 0) == 0) ++n;
        }
        return n;
    }

    std::filesystem::path base_;
    std::optional<EnvGuard> guard_;
    bool dbOk_ = false;
};

TEST_F(RecipeImageLifecycleDbTest, 封面上传落盘更新DB并清理临时源) {
    const int userId = ensureUser();
    const int recipeId = createRecipe(userId, "[]");
    const std::string src = writeTempSource("cover");

    PgRecipeRepository repo(testPool());
    const std::string url = repo.updateRecipeImage(userId, recipeId, src);

    const std::string prefix = "/uploads/recipes/recipe_" + std::to_string(recipeId) + "_";
    EXPECT_EQ(prefix, url.substr(0, prefix.size()));
    EXPECT_TRUE(std::filesystem::exists(UploadPaths::urlToPath(url, "recipes")));
    EXPECT_EQ(url, getImageUrl(recipeId));
    EXPECT_FALSE(std::filesystem::exists(src));  // 临时源文件已清理
}

TEST_F(RecipeImageLifecycleDbTest, 步骤图写入对应步骤并落盘清理) {
    const int userId = ensureUser();
    const int recipeId = createRecipe(userId,
        R"([{"order":1,"description":"a"},{"order":2,"description":"b"}])");
    const std::string src = writeTempSource("step");

    PgRecipeRepository repo(testPool());
    const std::string url = repo.updateStepImage(userId, recipeId, 1, src);

    const std::string prefix = "/uploads/recipes/recipe_" + std::to_string(recipeId) + "_step_1_";
    EXPECT_EQ(prefix, url.substr(0, prefix.size()));
    EXPECT_TRUE(std::filesystem::exists(UploadPaths::urlToPath(url, "recipes")));

    const auto steps = nlohmann::json::parse(getStepsJson(recipeId));
    ASSERT_EQ(2u, steps.size());
    EXPECT_EQ(url, steps[1]["image_url"].get<std::string>());
    EXPECT_FALSE(steps[0].contains("image_url"));  // 只写目标步骤
    EXPECT_FALSE(std::filesystem::exists(src));    // 临时源文件已清理
}

TEST_F(RecipeImageLifecycleDbTest, 越权返回403且无文件残留) {
    const int userId = ensureUser();
    const int recipeId = createRecipe(userId, "[]");
    const std::string src = writeTempSource("forbidden");

    PgRecipeRepository repo(testPool());
    try {
        repo.updateRecipeImage(userId + 100000, recipeId, src);  // 非作者
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(403, e.statusCode());
    }
    EXPECT_FALSE(std::filesystem::exists(src));  // 临时源文件已清理
    EXPECT_EQ(0u, countUploadsWithPrefix("recipe_" + std::to_string(recipeId) + "_"));  // 未落盘
}

TEST_F(RecipeImageLifecycleDbTest, 菜谱不存在返回404且无文件残留) {
    const int userId = ensureUser();
    const std::string src = writeTempSource("missing");

    PgRecipeRepository repo(testPool());
    try {
        repo.updateRecipeImage(userId, 99999999, src);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(404, e.statusCode());
    }
    EXPECT_FALSE(std::filesystem::exists(src));  // 复制前失败，临时源文件已清理
}

TEST_F(RecipeImageLifecycleDbTest, 步骤索引越界返回400且无文件残留) {
    const int userId = ensureUser();
    const int recipeId = createRecipe(userId, R"([{"order":1,"description":"a"}])");
    const std::string src = writeTempSource("bounds");

    PgRecipeRepository repo(testPool());
    try {
        repo.updateStepImage(userId, recipeId, 5, src);  // 越界
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(400, e.statusCode());
    }
    EXPECT_FALSE(std::filesystem::exists(src));  // 校验先于复制，临时源文件已清理
    EXPECT_EQ(0u, countUploadsWithPrefix("recipe_" + std::to_string(recipeId) + "_"));
}
