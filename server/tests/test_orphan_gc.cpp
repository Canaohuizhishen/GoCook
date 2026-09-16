// test_orphan_gc.cpp —— UploadOrphanGc（上传孤儿文件 GC）单元测试
//
// 覆盖（纯文件系统语义，不依赖数据库）：
//   · collectCandidates：老且未引用 → 候选；被引用 / 未过宽限期 → 保留；目录与符号链接 → 跳过
//   · removeCandidates：正常删除；二次核对（收集后被引用）→ 救回不删
//   · 目录不存在 → 空结果不崩溃
//
// mtime 通过 last_write_time 直接回拨，宽限期传入手工值，测试不依赖真实时间流逝。

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "../common/UploadOrphanGc.h"

using namespace std::chrono_literals;

namespace {

class OrphanGcTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        base_ = std::filesystem::temp_directory_path()
              / (std::string("gocook_orphan_gc_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
        std::filesystem::create_directories(base_ / "avatars");
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(base_, ec);
    }

    std::string avatarsDir() const {
        return (base_ / "avatars").string();
    }

    /// 在 avatars/ 下写文件并把 mtime 回拨 age（小时）
    void writeFile(const std::string& filename, std::chrono::hours age) {
        const auto path = base_ / "avatars" / filename;
        std::ofstream(path.string(), std::ios::binary) << "test-content";
        std::filesystem::last_write_time(
            path, std::filesystem::file_time_type::clock::now() - age);
    }

    std::filesystem::path base_;
};

} // namespace

// ==================== 阶段一：候选收集 ====================

TEST_F(OrphanGcTest, 收集_老且未引用为候选) {
    writeFile("user_1_100.jpg", 25h);
    UploadOrphanGc::GcResult stats;

    auto candidates = UploadOrphanGc::collectCandidates(
        avatarsDir(), {"user_2_999.png"}, 24h, stats);

    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].filename().string(), "user_1_100.jpg");
    EXPECT_EQ(stats.scanned, 1u);
    EXPECT_EQ(stats.kept, 0u);
}

TEST_F(OrphanGcTest, 收集_被引用保留) {
    writeFile("user_1_100.jpg", 25h);
    UploadOrphanGc::GcResult stats;

    auto candidates = UploadOrphanGc::collectCandidates(
        avatarsDir(), {"user_1_100.jpg"}, 24h, stats);

    EXPECT_TRUE(candidates.empty());
    EXPECT_EQ(stats.kept, 1u);
}

TEST_F(OrphanGcTest, 收集_未过宽限期保留) {
    writeFile("user_1_new.jpg", 1h);
    UploadOrphanGc::GcResult stats;

    auto candidates = UploadOrphanGc::collectCandidates(
        avatarsDir(), {}, 24h, stats);

    EXPECT_TRUE(candidates.empty());
    EXPECT_EQ(stats.kept, 1u);
}

TEST_F(OrphanGcTest, 收集_目录与符号链接跳过) {
    writeFile("user_1_old.jpg", 25h);
    std::filesystem::create_directories(base_ / "avatars" / "subdir");
    std::ofstream(base_ / "target.txt") << "target";
    std::filesystem::create_symlink(base_ / "target.txt",
                                    base_ / "avatars" / "link_old.jpg");
    UploadOrphanGc::GcResult stats;

    auto candidates = UploadOrphanGc::collectCandidates(
        avatarsDir(), {}, 24h, stats);

    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].filename().string(), "user_1_old.jpg");
    EXPECT_EQ(stats.scanned, 3u);
    EXPECT_EQ(stats.skipped, 2u);  // 目录 + 符号链接
    // 符号链接与目录必须原地保留（清理器不穿透、不删目录）
    EXPECT_TRUE(std::filesystem::exists(base_ / "avatars" / "subdir"));
    EXPECT_TRUE(std::filesystem::is_symlink(base_ / "avatars" / "link_old.jpg"));
}

TEST_F(OrphanGcTest, 收集_目录不存在返回空) {
    UploadOrphanGc::GcResult stats;
    auto candidates = UploadOrphanGc::collectCandidates(
        (base_ / "not_exist").string(), {}, 24h, stats);
    EXPECT_TRUE(candidates.empty());
}

// ==================== 阶段二：删除与二次核对 ====================

TEST_F(OrphanGcTest, 删除_候选正常回收) {
    writeFile("user_1_old.jpg", 25h);
    UploadOrphanGc::GcResult stats;
    auto candidates = UploadOrphanGc::collectCandidates(avatarsDir(), {}, 24h, stats);
    ASSERT_EQ(candidates.size(), 1u);

    UploadOrphanGc::removeCandidates(candidates, {}, stats);

    EXPECT_EQ(stats.deleted, 1u);
    EXPECT_EQ(stats.failed, 0u);
    EXPECT_FALSE(std::filesystem::exists(base_ / "avatars" / "user_1_old.jpg"));
}

TEST_F(OrphanGcTest, 删除_二次核对救回被绑定文件) {
    writeFile("user_1_old.jpg", 25h);
    UploadOrphanGc::GcResult stats;
    auto candidates = UploadOrphanGc::collectCandidates(avatarsDir(), {}, 24h, stats);
    ASSERT_EQ(candidates.size(), 1u);

    // 收集之后、删除之前，该文件被保存绑定（出现在二次快照中）→ 必须救回
    UploadOrphanGc::removeCandidates(candidates, {"user_1_old.jpg"}, stats);

    EXPECT_EQ(stats.deleted, 0u);
    EXPECT_EQ(stats.kept, 1u);
    EXPECT_TRUE(std::filesystem::exists(base_ / "avatars" / "user_1_old.jpg"));
}
