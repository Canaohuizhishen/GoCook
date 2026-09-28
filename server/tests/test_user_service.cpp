#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <jwt-cpp/jwt.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include "../services/UserServiceImpl.h"
#include "MockUserRepository.h"
#include "../common/EmailSender.h"
#include "bcrypt/crypt_blowfish.h"

using namespace testing;
using namespace gocook::models;
using namespace gocook::services;
using namespace gocook::repository;

namespace {
    const std::string TEST_JWT_SECRET = "test-jwt-secret-for-unit-tests";

    RegisterRequest makeRegisterReq() {
        return {"testuser", "password123", "test@example.com"};
    }

    LoginRequest makeLoginReq() {
        return {"testuser", "password123"};
    }

    UserProfile makeUserProfile(int id = 42) {
        return {id, "testuser", "Test User", "test@example.com",
                "13800138000", "http://example.com/avatar.png", true, "2026-01-01"};
    }

    // 环境变量守卫：用例内 set，作用域结束恢复原值（与 test_upload_paths.cpp 同一模式）
    class EnvGuard {
    public:
        explicit EnvGuard(const char* name) : name_(name) {
            if (const char* v = std::getenv(name); v != nullptr)
                saved_ = std::string(v);
        }
        ~EnvGuard() {
            if (saved_) setenv(name_.c_str(), saved_->c_str(), 1);
            else unsetenv(name_.c_str());
        }
        void set(const char* value) { setenv(name_.c_str(), value, 1); }
        void unset() { unsetenv(name_.c_str()); }
    private:
        std::string name_;
        std::optional<std::string> saved_;
    };

    /// 建独立临时上传根目录，并可选写入一个托管头像文件（<base>/avatars/<filename>）
    std::filesystem::path makeUploadsRoot(const std::string& tag,
                                          const std::string& filename = "") {
        auto base = std::filesystem::temp_directory_path() / ("gocook_avatar_bind_" + tag);
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "avatars");
        if (!filename.empty()) {
            std::ofstream ofs(base / "avatars" / filename, std::ios::binary);
            ofs << "x";
        }
        return base;
    }
}

// ==================== 注册 ====================

TEST(UserServiceTest, 注册成功进入待验证并发送验证码) {
    // 清除 SMTP 环境变量，模拟开发模式（邮件落日志，不真实发送）；EnvGuard 作用域结束自动恢复
    EnvGuard guardUser("GOCOOK_SMTP_USER");
    EnvGuard guardPass("GOCOOK_SMTP_PASS");
    guardUser.unset();
    guardPass.unset();

    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto req = makeRegisterReq();

    // 两段式注册第一步：不建号，写入待验证记录（验证码为 6 位数字）
    EXPECT_CALL(*repo, findByUsername("testuser")).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, existsByEmail("test@example.com")).WillOnce(Return(false));
    EXPECT_CALL(*repo, createUser(_, _, _)).Times(0);
    EXPECT_CALL(*repo, upsertPendingRegistration("testuser", _, "test@example.com", _))
        .WillOnce([](const std::string&, const std::string&, const std::string&, const std::string& token) {
            EXPECT_EQ(token.size(), 6u);
        });

    EXPECT_NO_THROW(service.registerUser(req));
}

TEST(UserServiceTest, 注册邮箱已注册静默成功不建号) {
    // 清除 SMTP 环境变量，模拟开发模式；EnvGuard 作用域结束自动恢复
    EnvGuard guardUser("GOCOOK_SMTP_USER");
    EnvGuard guardPass("GOCOOK_SMTP_PASS");
    guardUser.unset();
    guardPass.unset();

    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findByUsername(_)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, existsByEmail(_)).WillRepeatedly(Return(true));
    // 防枚举：不抛异常、不建号、不写待验证记录，差异只体现在邮件内容
    EXPECT_CALL(*repo, upsertPendingRegistration(_, _, _, _)).Times(0);

    EXPECT_NO_THROW(service.registerUser(makeRegisterReq()));
}

TEST(UserServiceTest, 注册用户名冲突) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    // 新顺序：先查用户名（409 保持），命中即返回，不再经过邮箱检查
    EXPECT_CALL(*repo, findByUsername(_)).WillOnce(Return(std::optional<UserAuthInfo>(UserAuthInfo{})));

    try {
        service.registerUser(makeRegisterReq());
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "用户名已存在");
    }
}

// ==================== 注册验证（两段式第二步） ====================

TEST(UserServiceTest, 注册验证成功建号) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, createUserFromPendingRegistration("test@example.com", "123456"))
        .WillOnce(Return(RegistrationOutcome::Success));

    EXPECT_NO_THROW(service.verifyRegistration("test@example.com", "123456"));
}

TEST(UserServiceTest, 注册验证码无效或过期) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, createUserFromPendingRegistration(_, _))
        .WillOnce(Return(RegistrationOutcome::InvalidCode));

    try {
        service.verifyRegistration("test@example.com", "000000");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "验证码无效或已过期");
    }
}

TEST(UserServiceTest, 注册验证用户名被占用) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, createUserFromPendingRegistration(_, _))
        .WillOnce(Return(RegistrationOutcome::UsernameTaken));

    try {
        service.verifyRegistration("test@example.com", "123456");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "用户名已被占用，请更换用户名后重新注册");
    }
}

TEST(UserServiceTest, 注册验证邮箱已被注册) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, createUserFromPendingRegistration(_, _))
        .WillOnce(Return(RegistrationOutcome::EmailTaken));

    try {
        service.verifyRegistration("test@example.com", "123456");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "该邮箱已被注册，请直接登录");
    }
}

// ==================== 登录 ====================

TEST(UserServiceTest, 登录成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", "", "user"}));

    EXPECT_THROW(service.login(makeLoginReq()), ServiceException);
}

TEST(UserServiceTest, 登录验证密码) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    char salt_buf[128];
    char hash_buf[128];
    const char fixed_input[16] = {};
    char* salt = _crypt_gensalt_blowfish_rn(
        "$2a$", 4, fixed_input, 16, salt_buf, sizeof(salt_buf));
    ASSERT_NE(salt, nullptr);
    char* hashed = _crypt_blowfish_rn("password123", salt, hash_buf, sizeof(hash_buf));
    ASSERT_NE(hashed, nullptr);
    std::string knownHash(hashed);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", knownHash, "user"}));

    LoginResponse resp = service.login({"testuser", "password123"});
    EXPECT_GT(resp.token.size(), 0);
    EXPECT_EQ(resp.user_id, 42);
    EXPECT_EQ(resp.username, "testuser");
}

TEST(UserServiceTest, JWT令牌校验) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    char salt_buf[128], hash_buf[128];
    const char fi[16] = {};
    char* s = _crypt_gensalt_blowfish_rn("$2a$", 4, fi, 16, salt_buf, sizeof(salt_buf));
    ASSERT_NE(s, nullptr);
    char* hh = _crypt_blowfish_rn("pwd", s, hash_buf, sizeof(hash_buf));
    ASSERT_NE(hh, nullptr);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", std::string(hh), "user"}));

    LoginResponse resp = service.login({"testuser", "pwd"});

    auto decoded = jwt::decode(resp.token);
    auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{TEST_JWT_SECRET});
    EXPECT_NO_THROW(verifier.verify(decoded));

    EXPECT_EQ(decoded.get_payload_claim("userId").as_string(), "42");
    EXPECT_EQ(decoded.get_payload_claim("username").as_string(), "testuser");
    EXPECT_EQ(decoded.get_payload_claim("role").as_string(), "user");
}

// 签发侧 ver 写入（审查跟进）：登录签发的令牌必须携带当前会话版本——中间件据此与
// users.token_version 比对做主动吊销（覆盖配对见 test_token_revocation_integration.cpp）
TEST(UserServiceTest, 登录令牌携带当前会话版本ver) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    char salt_buf[128], hash_buf[128];
    const char fi[16] = {};
    char* s = _crypt_gensalt_blowfish_rn("$2a$", 4, fi, 16, salt_buf, sizeof(salt_buf));
    ASSERT_NE(s, nullptr);
    char* hh = _crypt_blowfish_rn("password123", s, hash_buf, sizeof(hash_buf));
    ASSERT_NE(hh, nullptr);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", std::string(hh), "user"}));
    EXPECT_CALL(*repo, getTokenVersion(42)).WillOnce(Return(std::optional<int>{3}));

    LoginResponse resp = service.login({"testuser", "password123"});

    auto decoded = jwt::decode(resp.token);
    auto verifier = jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{TEST_JWT_SECRET});
    EXPECT_NO_THROW(verifier.verify(decoded));
    ASSERT_TRUE(decoded.has_payload_claim("ver"));
    EXPECT_EQ(decoded.get_payload_claim("ver").as_string(), "3");
}

// 版本查询缺失（用户被并发删除等竞态）→ ver 按 0 兜底签发（.value_or(0) 兼容路径）；
// 该令牌随后会被中间件按「用户不存在」拒绝，不构成放行缺口
TEST(UserServiceTest, 登录令牌版本查询缺失ver按零兜底) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    char salt_buf[128], hash_buf[128];
    const char fi[16] = {};
    char* s = _crypt_gensalt_blowfish_rn("$2a$", 4, fi, 16, salt_buf, sizeof(salt_buf));
    ASSERT_NE(s, nullptr);
    char* hh = _crypt_blowfish_rn("password123", s, hash_buf, sizeof(hash_buf));
    ASSERT_NE(hh, nullptr);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", std::string(hh), "user"}));
    EXPECT_CALL(*repo, getTokenVersion(42)).WillOnce(Return(std::nullopt));

    LoginResponse resp = service.login({"testuser", "password123"});

    auto decoded = jwt::decode(resp.token);
    ASSERT_TRUE(decoded.has_payload_claim("ver"));
    EXPECT_EQ(decoded.get_payload_claim("ver").as_string(), "0");
}

TEST(UserServiceTest, 登录用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findByUsername("nonexistent"))
        .WillOnce(Return(std::nullopt));

    try {
        service.login({"nonexistent", "password123"});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 401);
    }
}

TEST(UserServiceTest, 登录密码错误) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findByUsername("testuser"))
        .WillOnce(Return(UserAuthInfo{42, "testuser", "badhash", "user"}));

    try {
        service.login({"testuser", "password123"});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 401);
    }
}

// ==================== 获取用户信息 ====================

TEST(UserServiceTest, 获取当前用户成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto expected = makeUserProfile();
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(expected));

    auto result = service.getCurrentUser(42);
    EXPECT_EQ(result.id, 42);
    EXPECT_EQ(result.username, "testuser");
    EXPECT_EQ(result.display_name, "Test User");
    EXPECT_EQ(result.email, "test@example.com");
}

TEST(UserServiceTest, 获取用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));

    try {
        service.getCurrentUser(999);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 健康指标 ====================

TEST(UserServiceTest, 录入健康指标成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, updateHealthProfile(42, _)).Times(1);

    HealthProfileRequest req;
    req.height_cm = 175;
    req.weight_kg = 70.0;
    req.conditions = {"高血压", "高血脂", "痛风"};

    auto resp = service.updateHealthProfile(42, req);
    EXPECT_GT(resp.suggested_avoidances.size(), 0);

    // 验证高血压忌口建议（两档口径：硬排除高盐加工品 + 调味料仅提示，对齐引擎黑名单）
    bool hasHardExclusion = false;
    bool hasSoftAdvice = false;
    for (const auto& item : resp.suggested_avoidances) {
        if (item.ingredient == "咸菜、腊肉、咸鱼、腐乳、榨菜") {
            hasHardExclusion = true;
            EXPECT_EQ(item.reason, "高盐加工品，推荐结果将直接排除含此类食材的菜谱");
        } else if (item.ingredient == "盐、酱油、豆瓣酱") {
            hasSoftAdvice = true;
            EXPECT_EQ(item.reason, "调味高钠，系统仅提示少放，不屏蔽菜谱");
        }
    }
    EXPECT_TRUE(hasHardExclusion);
    EXPECT_TRUE(hasSoftAdvice);

    // 高血脂/痛风：文案须与引擎硬档名单逐字一致（审查修复：原文案含引擎名单外条目"动物内脏"、
    // 漏掉 猪油/黄油/奶油/猪板油/浓汤/香菇/虾/蟹 等）
    bool hasHyperlipidList = false;
    bool hasGoutList = false;
    for (const auto& item : resp.suggested_avoidances) {
        if (item.ingredient == "肥肉、猪油、黄油、奶油、五花肉、油炸、猪板油") {
            hasHyperlipidList = true;
            EXPECT_EQ(item.reason, "高脂食材，推荐结果将直接排除含此类食材的菜谱");
        } else if (item.ingredient == "海鲜、动物内脏、啤酒、浓汤、香菇、虾、蟹") {
            hasGoutList = true;
            EXPECT_EQ(item.reason, "高嘌呤/高风险食材，推荐结果将直接排除含此类食材的菜谱");
        }
    }
    EXPECT_TRUE(hasHyperlipidList);
    EXPECT_TRUE(hasGoutList);
}

TEST(UserServiceTest, 录入健康指标用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, updateHealthProfile(_, _)).Times(0);

    HealthProfileRequest req;
    req.height_cm = 175;
    req.weight_kg = 70.0;

    try {
        service.updateHealthProfile(999, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 未实现的方法 ====================

// ==================== 密码重置 ====================

TEST(UserServiceTest, 请求重置密码用户名邮箱不匹配报错不发信) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findIdByUsernameAndEmail("unknown_user", "unreg@test.com"))
        .WillOnce(Return(std::nullopt));
    // 双字段匹配是发信门槛：不匹配不发信、不建令牌
    EXPECT_CALL(*repo, createPasswordResetToken(_, _)).Times(0);

    try {
        service.requestPasswordReset("unknown_user", "unreg@test.com");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "用户名或邮箱不正确");
    }
}

TEST(UserServiceTest, 请求重置密码邮箱已注册生成令牌) {
    // 清除 SMTP 环境变量，模拟开发模式；EnvGuard 作用域结束自动恢复
    EnvGuard guardUser("GOCOOK_SMTP_USER");
    EnvGuard guardPass("GOCOOK_SMTP_PASS");
    guardUser.unset();
    guardPass.unset();

    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    std::string issuedToken;
    EXPECT_CALL(*repo, findIdByUsernameAndEmail("testuser", "user@test.com"))
        .WillOnce(Return(std::optional<int>(42)));
    EXPECT_CALL(*repo, createPasswordResetToken(42, _))
        .WillOnce(SaveArg<1>(&issuedToken));

    // SMTP 未配置（开发模式）：不抛异常、不返回令牌；验证码经 [DEV MAIL] 打到服务端日志；令牌 6 位数字并在仓库边界入库
    service.requestPasswordReset("testuser", "user@test.com");
    EXPECT_EQ(issuedToken.size(), 6);   // 6 位数字验证码
    EXPECT_TRUE(std::all_of(issuedToken.begin(), issuedToken.end(),
                            [](char c) { return c >= '0' && c <= '9'; }));
}

TEST(UserServiceTest, 重置密码令牌无效) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findUserIdByResetToken("bad-token")).WillOnce(Return(std::nullopt));

    try {
        service.resetPassword("bad-token", "NewPass123");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        // 应该包含基础错误信息，可能附加调试详情
        EXPECT_TRUE(std::string(e.what()).find("令牌无效或已过期") != std::string::npos);
    }
}

TEST(UserServiceTest, 重置密码成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findUserIdByResetToken("valid-token")).WillOnce(Return(std::optional<int>(42)));
    EXPECT_CALL(*repo, resetPasswordAndMarkTokenUsed(42, _, "valid-token")).Times(1);

    EXPECT_NO_THROW(service.resetPassword("valid-token", "NewPass123"));
}

// ==================== 更新个人资料 ====================

TEST(UserServiceTest, 更新个人资料成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    UpdateProfileRequest profile;
    profile.display_name = "新昵称";
    profile.email = "new@example.com";
    profile.phone = "13900139000";

    auto currentProfile = makeUserProfile(42);  // 该资料带邮箱 "test@example.com"
    auto updatedProfile = makeUserProfile(42);
    updatedProfile.display_name = "新昵称";
    updatedProfile.email = "new@example.com";
    updatedProfile.phone = "13900139000";

    // 第一次 findById 用于邮箱查重（返回当前资料），第二次用于返回更新后的资料
    EXPECT_CALL(*repo, findById(42))
        .WillOnce(Return(currentProfile))
        .WillOnce(Return(updatedProfile));
    EXPECT_CALL(*repo, existsByEmail("new@example.com")).WillOnce(Return(false));
    EXPECT_CALL(*repo, updateProfile(42, _)).Times(1);

    auto result = service.updateProfile(42, profile);
    EXPECT_EQ(result.display_name, "新昵称");
    EXPECT_EQ(result.email, "new@example.com");
}

TEST(UserServiceTest, 更新个人资料没有字段) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    try {
        service.updateProfile(1, {});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
    }
}

TEST(UserServiceTest, 更新个人资料昵称为空) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    UpdateProfileRequest profile;
    profile.display_name = "";

    try {
        service.updateProfile(1, profile);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "昵称不能为空");
    }
}

TEST(UserServiceTest, 更新个人资料邮箱已注册) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto current = makeUserProfile(42);
    current.email = "old@example.com";

    UpdateProfileRequest profile;
    profile.email = "taken@example.com";

    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(current));
    EXPECT_CALL(*repo, existsByEmail("taken@example.com")).WillOnce(Return(true));

    try {
        service.updateProfile(42, profile);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "邮箱已被注册");
    }
}

TEST(UserServiceTest, 更新个人资料邮箱不变跳过检查) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto current = makeUserProfile(42);
    current.email = "same@example.com";

    UpdateProfileRequest profile;
    profile.display_name = "新昵称";
    profile.email = "same@example.com";  // 与当前邮箱相同 — 不应调用 existsByEmail

    EXPECT_CALL(*repo, findById(42))
        .Times(2)
        .WillRepeatedly(Return(current));
    EXPECT_CALL(*repo, existsByEmail(_)).Times(0);  // 不应检查邮箱唯一性
    EXPECT_CALL(*repo, updateProfile(42, _)).Times(1);

    auto result = service.updateProfile(42, profile);
    EXPECT_EQ(result.display_name, "Test User");
}

// ==================== 头像上传 ====================

TEST(UserServiceTest, 上传头像成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    AvatarUploadResponse expectedResp;
    expectedResp.avatar_url = "/uploads/avatars/user_42_12345.jpg";

    EXPECT_CALL(*repo, uploadAvatar(42, "/tmp/test_avatar.jpg"))
        .WillOnce(Return(expectedResp));

    auto result = service.uploadAvatar(42, "/tmp/test_avatar.jpg");
    EXPECT_EQ(result.avatar_url, "/uploads/avatars/user_42_12345.jpg");
}

TEST(UserServiceTest, 上传头像空路径) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    try {
        service.uploadAvatar(1, "");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
    }
}

// ==================== 头像绑定（保存时生效） ====================

TEST(UserServiceTest, 更新资料绑定本人暂存头像成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    const auto base = makeUploadsRoot("bind_ok", "user_42_12345.jpg");
    EnvGuard guard("GOCOOK_UPLOADS_DIR");
    guard.set(base.c_str());

    UpdateProfileRequest profile;
    profile.avatar_url = "/uploads/avatars/user_42_12345.jpg";

    EXPECT_CALL(*repo, updateProfile(42, Field(&UpdateProfileRequest::avatar_url,
        Optional(std::string("/uploads/avatars/user_42_12345.jpg"))))).Times(1);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(makeUserProfile(42)));

    EXPECT_NO_THROW(service.updateProfile(42, profile));

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
}

TEST(UserServiceTest, 更新资料绑定他人前缀头像被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    UpdateProfileRequest profile;
    profile.avatar_url = "/uploads/avatars/user_99_12345.jpg";  // 非本人前缀

    try {
        service.updateProfile(42, profile);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "无效的头像引用");
    }
}

TEST(UserServiceTest, 更新资料绑定不存在暂存头像被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    const auto base = makeUploadsRoot("bind_missing");  // 空目录：文件不存在
    EnvGuard guard("GOCOOK_UPLOADS_DIR");
    guard.set(base.c_str());

    UpdateProfileRequest profile;
    profile.avatar_url = "/uploads/avatars/user_42_999.jpg";

    try {
        service.updateProfile(42, profile);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "头像文件不存在，请重新上传");
    }

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
}

TEST(UserServiceTest, 更新资料绑定外部链接放行) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    UpdateProfileRequest profile;
    profile.avatar_url = "https://cdn.example.com/avatar.png";

    EXPECT_CALL(*repo, updateProfile(42, _)).Times(1);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(makeUserProfile(42)));

    EXPECT_NO_THROW(service.updateProfile(42, profile));
}

TEST(UserServiceTest, 更新资料绑定非法链接格式被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    UpdateProfileRequest profile;
    profile.avatar_url = "ftp://cdn.example.com/avatar.png";

    try {
        service.updateProfile(42, profile);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "头像链接格式无效");
    }
}

// ==================== 放弃暂存头像 ====================

TEST(UserServiceTest, 放弃暂存头像删除文件) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    const auto base = makeUploadsRoot("discard_ok", "user_42_111.jpg");
    EnvGuard guard("GOCOOK_UPLOADS_DIR");
    guard.set(base.c_str());

    auto user = makeUserProfile(42);  // 当前头像指向别的文件
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(user));

    EXPECT_NO_THROW(service.discardPendingAvatar(42, "/uploads/avatars/user_42_111.jpg"));
    EXPECT_FALSE(std::filesystem::exists(base / "avatars" / "user_42_111.jpg"));

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
}

TEST(UserServiceTest, 放弃暂存_当前使用中头像被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto user = makeUserProfile(42);
    user.avatar_url = "/uploads/avatars/user_42_111.jpg";
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(user));

    try {
        service.discardPendingAvatar(42, "/uploads/avatars/user_42_111.jpg");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "该头像正在使用中，无法放弃");
    }
}

TEST(UserServiceTest, 放弃暂存_他人前缀被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    try {
        service.discardPendingAvatar(42, "/uploads/avatars/user_99_111.jpg");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "无效的头像引用");
    }
}

// ==================== 注销账户 ====================

TEST(UserServiceTest, 注销账户成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, deleteAccount(42)).Times(1);

    EXPECT_NO_THROW(service.deleteAccount(42));
}

TEST(UserServiceTest, 注销账户用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, deleteAccount(_)).Times(0);

    try {
        service.deleteAccount(999);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 获取饮食偏好 ====================

TEST(UserServiceTest, 获取饮食偏好成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    UserPreferences expectedPrefs;
    expectedPrefs.likes = {"中式", "清淡"};
    expectedPrefs.dislikes = {"香菜"};
    expectedPrefs.health_goal = "减脂";
    EXPECT_CALL(*repo, getPreferences(42)).WillOnce(Return(expectedPrefs));

    auto result = service.getPreferences(42);
    EXPECT_EQ(result.likes.size(), 2u);
    EXPECT_EQ(result.likes[0], "中式");
    EXPECT_EQ(result.likes[1], "清淡");
    EXPECT_EQ(result.dislikes.size(), 1u);
    EXPECT_EQ(result.dislikes[0], "香菜");
    EXPECT_EQ(result.health_goal, "减脂");
}

TEST(UserServiceTest, 获取饮食偏好用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, getPreferences(_)).Times(0);

    try {
        service.getPreferences(999);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 更新饮食偏好 ====================

TEST(UserServiceTest, 更新饮食偏好成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    UserPreferences prefs;
    prefs.likes = {"中式", "清淡"};
    prefs.dislikes = {"香菜"};
    prefs.health_goal = "减脂";
    EXPECT_CALL(*repo, updatePreferences(42, _)).Times(1);

    EXPECT_NO_THROW(service.updatePreferences(42, prefs));
}

TEST(UserServiceTest, 更新饮食偏好用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, updatePreferences(_, _)).Times(0);

    try {
        service.updatePreferences(999, {});
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 修改密码 ====================

namespace {
    // 生成已知密码的 bcrypt 哈希（用于修改密码测试）
    std::string makeBcryptHash(const std::string& plain) {
        char salt_buf[128], hash_buf[128];
        const char fi[16] = {};
        char* s = _crypt_gensalt_blowfish_rn("$2a$", 4, fi, 16, salt_buf, sizeof(salt_buf));
        char* h = _crypt_blowfish_rn(plain.c_str(), s, hash_buf, sizeof(hash_buf));
        return std::string(h ? h : "");
    }
}

TEST(UserServiceTest, 修改密码成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    std::string oldHash = makeBcryptHash("OldPass123");
    EXPECT_CALL(*repo, getPasswordHash(42)).WillOnce(Return(oldHash));
    EXPECT_CALL(*repo, changePassword(42, _)).Times(1);

    EXPECT_NO_THROW(service.changePassword(42, "OldPass123", "NewPass456"));
}

TEST(UserServiceTest, 修改密码原密码错误) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    std::string oldHash = makeBcryptHash("CorrectOld123");
    EXPECT_CALL(*repo, getPasswordHash(42)).WillOnce(Return(oldHash));
    EXPECT_CALL(*repo, changePassword(42, _)).Times(0);

    try {
        service.changePassword(42, "WrongOld456", "NewPass789");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "原密码不正确");
    }
}

TEST(UserServiceTest, 修改密码新密码太短) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    std::string oldHash = makeBcryptHash("OldPass123");
    EXPECT_CALL(*repo, getPasswordHash(42)).WillOnce(Return(oldHash));
    EXPECT_CALL(*repo, changePassword(42, _)).Times(0);

    try {
        service.changePassword(42, "OldPass123", "Ab1");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "密码需包含字母和数字，至少6位");
    }
}

// ==================== 获取健康指标 ====================

TEST(UserServiceTest, 获取健康指标成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    HealthProfileResponse resp;
    resp.height_cm = 175;
    resp.weight_kg = 70.0;
    resp.conditions = {"高血压"};
    EXPECT_CALL(*repo, getHealthProfile(42)).WillOnce(Return(resp));

    auto result = service.getHealthProfile(42);
    EXPECT_EQ(result.height_cm, 175);
    EXPECT_EQ(result.weight_kg, 70.0);
    EXPECT_EQ(result.conditions.size(), 1u);
    EXPECT_GT(result.suggested_avoidances.size(), 0u);  // 应生成忌口建议
}

TEST(UserServiceTest, 获取健康指标用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, getHealthProfile(_)).Times(0);

    try {
        service.getHealthProfile(999);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 收藏列表 ====================

TEST(UserServiceTest, 获取收藏列表成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    FavoriteItem item;
    item.id = 1;
    item.recipe_id = 100;
    item.name = "红烧肉";
    item.group_name = "默认收藏夹";
    item.is_public = true;
    PagedFavorites expected;
    expected.data = {item};
    expected.pagination = {1, 1, 1, 20};

    EXPECT_CALL(*repo, getFavorites(42, 1, 20, std::string(""))).WillOnce(Return(expected));

    auto result = service.getFavorites(42, 1, 20);
    EXPECT_EQ(result.data.size(), 1u);
    EXPECT_EQ(result.data[0].name, "红烧肉");
    EXPECT_EQ(result.data[0].group_name, "默认收藏夹");
}

TEST(UserServiceTest, 获取收藏列表用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, getFavorites(999, _, _, _)).Times(0);

    try {
        service.getFavorites(999, 1, 20);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 收藏分组 ====================

TEST(UserServiceTest, 获取收藏分组列表成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    std::vector<FavoriteGroup> groups;
    groups.push_back({1, "默认收藏夹", 0, 3});
    groups.push_back({2, "减脂餐", 1, 1});
    EXPECT_CALL(*repo, getFavoriteGroups(42)).WillOnce(Return(groups));

    auto result = service.getFavoriteGroups(42);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].name, "默认收藏夹");
    EXPECT_EQ(result[0].count, 3);
    EXPECT_EQ(result[1].name, "减脂餐");
}

TEST(UserServiceTest, 创建收藏分组成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    FavoriteGroup newGroup{3, "甜点", 2, 0};
    EXPECT_CALL(*repo, createFavoriteGroup(42, _)).WillOnce(Return(newGroup));

    CreateGroupRequest req{"甜点"};
    auto result = service.createFavoriteGroup(42, req);
    EXPECT_EQ(result.id, 3);
    EXPECT_EQ(result.name, "甜点");
}

TEST(UserServiceTest, 删除收藏分组成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, deleteFavoriteGroup(42, 2)).Times(1);
    EXPECT_NO_THROW(service.deleteFavoriteGroup(42, 2));
}

// ==================== 收藏项操作 ====================

TEST(UserServiceTest, 批量更新收藏项属性成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    BatchUpdateFavoritesRequest req;
    req.favorite_ids = {1, 2};
    req.group_id = 3;
    req.is_public = false;
    EXPECT_CALL(*repo, batchUpdateFavorites(42, _)).Times(1);

    EXPECT_NO_THROW(service.batchUpdateFavorites(42, req));
}

TEST(UserServiceTest, 批量删除收藏成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    BatchDeleteFavoritesRequest req{{1, 2, 3}};
    EXPECT_CALL(*repo, batchDeleteFavorites(42, _)).Times(1);

    EXPECT_NO_THROW(service.batchDeleteFavorites(42, req));
}

// ==================== 通知 ====================

TEST(UserServiceTest, 获取通知列表成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    NotificationItem notif;
    notif.id = 101;
    notif.title = "审核结果";
    notif.content = "您的菜谱已通过审核";
    notif.type = "review";
    notif.is_read = false;   // 由仓库按"id > 水位"动态计算，服务层透传
    PagedNotifications expected;
    expected.data = {notif};
    expected.pagination = {1, 1, 1, 20};

    EXPECT_CALL(*repo, getNotifications(42, 1, 20, std::string(""))).WillOnce(Return(expected));

    auto result = service.getNotifications(42, 1, 20);
    EXPECT_EQ(result.data.size(), 1u);
    EXPECT_EQ(result.data[0].title, "审核结果");
    EXPECT_EQ(result.data[0].type, "review");
    EXPECT_FALSE(result.data[0].is_read);
}

TEST(UserServiceTest, 未读汇总成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));

    NotificationUnreadSummary expected;
    expected.unread_review = 3;
    expected.unread_interaction = 0;
    expected.has_new_announcement = true;
    EXPECT_CALL(*repo, getUnreadSummary(42)).WillOnce(Return(expected));

    auto result = service.getUnreadSummary(42);
    EXPECT_EQ(result.unread_review, 3);
    EXPECT_EQ(result.unread_interaction, 0);
    EXPECT_TRUE(result.has_new_announcement);
}

TEST(UserServiceTest, 上报通知已读水位成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, updateReadWatermark(42, std::string("review"), 301)).Times(1);

    EXPECT_NO_THROW(service.setNotificationsReadState(42, "review", 301));
}

TEST(UserServiceTest, 上报通知已读水位非法类型被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    // 非法类型在 findById 之前收口：不查用户、不进仓库
    EXPECT_CALL(*repo, findById(_)).Times(0);
    EXPECT_CALL(*repo, updateReadWatermark(_, _, _)).Times(0);

    try {
        service.setNotificationsReadState(42, "system", 301);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), "不支持的通知类型");
    }
}

TEST(UserServiceTest, 上报已读水位负数被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(_)).Times(0);
    EXPECT_CALL(*repo, updateReadWatermark(_, _, _)).Times(0);

    try {
        service.setAnnouncementsReadState(42, -1);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), "last_seen_id 不能为负");
    }
}

TEST(UserServiceTest, 上报公告已读水位成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, updateReadWatermark(42, std::string("announcement"), 5)).Times(1);

    EXPECT_NO_THROW(service.setAnnouncementsReadState(42, 5));
}

// ==================== 边界测试 ====================

TEST(UserServiceTest, 批量删除收藏空ID列表被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    // 空列表在 findById 之前收口（与批量更新对称）：不查用户、不进仓库
    EXPECT_CALL(*repo, findById(_)).Times(0);
    EXPECT_CALL(*repo, batchDeleteFavorites(42, _)).Times(0);

    BatchDeleteFavoritesRequest req{};
    try {
        service.batchDeleteFavorites(42, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), "favorite_ids 不能为空");
    }
}

TEST(UserServiceTest, 创建收藏分组名称重复) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, createFavoriteGroup(42, _))
        .WillOnce(Throw(ServiceException("分组名已存在", 409)));

    CreateGroupRequest req{"最爱菜品"};
    try {
        service.createFavoriteGroup(42, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
    }
}

TEST(UserServiceTest, 批量更新收藏项空ID列表被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, batchUpdateFavorites(42, _)).Times(0);

    BatchUpdateFavoritesRequest req;
    req.group_id = 3;
    try {
        service.batchUpdateFavorites(42, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), "favorite_ids 不能为空");
    }
}

TEST(UserServiceTest, 批量更新收藏项缺少更新字段被拒) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, batchUpdateFavorites(42, _)).Times(0);

    BatchUpdateFavoritesRequest req;
    req.favorite_ids = {1};
    try {
        service.batchUpdateFavorites(42, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_EQ(std::string(e.what()), "缺少更新字段");
    }
}

TEST(UserServiceTest, 获取收藏列表分页边界page为零) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, getFavorites(42, 0, 20, std::string(""))).Times(1);

    EXPECT_NO_THROW(service.getFavorites(42, 0, 20, ""));
}

TEST(UserServiceTest, 获取收藏列表分页边界page极大值) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, getFavorites(42, 999999, 20, std::string(""))).Times(1);

    EXPECT_NO_THROW(service.getFavorites(42, 999999, 20, ""));
}

TEST(UserServiceTest, 获取通知列表按类型过滤) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, getNotifications(42, 1, 20, std::string("review"))).Times(1);

    PagedNotifications result = service.getNotifications(42, 1, 20, "review");
}

// ==================== 以下为原有测试 ====================

TEST(UserServiceTest, 删除通知成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto profile = makeUserProfile(42);
    EXPECT_CALL(*repo, findById(42)).WillOnce(Return(profile));
    EXPECT_CALL(*repo, deleteNotification(42, 101)).Times(1);

    EXPECT_NO_THROW(service.deleteNotification(42, 101));
}

TEST(UserServiceTest, 删除通知用户不存在) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findById(999)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, deleteNotification(_, _)).Times(0);

    try {
        service.deleteNotification(999, 1);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 404);
        EXPECT_STREQ(e.what(), "用户不存在");
    }
}

// ==================== 密码重置 — SMTP 路径 ====================

class SmtpEnvironmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 覆盖 SMTP 环境变量（端口设为一个不可能的值，确保连接快速失败）
        // EnvGuard 成员构造时已存档原值、析构时恢复（无需 TearDown）
        smtpHost_.set("127.0.0.1");
        smtpPort_.set("1");
        smtpUser_.set("test@gocook.dev");
        smtpPass_.set("test-password");
        smtpFrom_.unset();
    }

private:
    EnvGuard smtpHost_{"GOCOOK_SMTP_HOST"};
    EnvGuard smtpPort_{"GOCOOK_SMTP_PORT"};
    EnvGuard smtpUser_{"GOCOOK_SMTP_USER"};
    EnvGuard smtpPass_{"GOCOOK_SMTP_PASS"};
    EnvGuard smtpFrom_{"GOCOOK_SMTP_FROM"};
};

TEST_F(SmtpEnvironmentTest, SMTP已配置时isConfigured返回true) {
    EXPECT_TRUE(EmailSender::isConfigured());
}

TEST_F(SmtpEnvironmentTest, SMTP已配置但不可达时requestPasswordReset抛出异常) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findIdByUsernameAndEmail("testuser", "test@example.com"))
        .WillOnce(Return(42));
    EXPECT_CALL(*repo, createPasswordResetToken(42, _)).Times(1);

    try {
        service.requestPasswordReset("testuser", "test@example.com");
        FAIL() << "Expected ServiceException for unreachable SMTP";
    } catch (const ServiceException& e) {
        // SMTP 不可达应触发 ServiceException（非 400，而是 SMTP 错误）
        EXPECT_EQ(e.statusCode(), 500);
    }
}

TEST_F(SmtpEnvironmentTest, SMTP已配置但发送失败时不回退开发模式) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, findIdByUsernameAndEmail("testuser", "test@example.com"))
        .WillOnce(Return(42));
    EXPECT_CALL(*repo, createPasswordResetToken(42, _)).Times(1);

    // SMTP 已配置但发送失败：抛 500 且文案与注册链路统一（而非静默降级为 [DEV MAIL] 开发模式日志）；
    // 与上一条「不可达…抛出异常」互补：该条钉住状态码语义，本条钉住失败文案契约
    try {
        service.requestPasswordReset("testuser", "test@example.com");
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 500);
        EXPECT_STREQ(e.what(), "邮件发送失败，请稍后再试或联系管理员");
    }
}
