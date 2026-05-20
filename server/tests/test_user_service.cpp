#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <jwt-cpp/jwt.h>
#include "../services/UserServiceImpl.h"
#include "MockUserRepository.h"
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
}

// ==================== 注册 ====================

TEST(UserServiceTest, 注册成功) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    auto req = makeRegisterReq();

    EXPECT_CALL(*repo, existsByEmail("test@example.com")).WillOnce(Return(false));
    EXPECT_CALL(*repo, findByUsername("testuser")).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*repo, createUser("testuser", _, "test@example.com")).Times(1);

    EXPECT_NO_THROW(service.registerUser(req));
}

TEST(UserServiceTest, 注册邮箱冲突) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, existsByEmail(_)).WillRepeatedly(Return(true));

    try {
        service.registerUser(makeRegisterReq());
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "邮箱已被注册");
    }
}

TEST(UserServiceTest, 注册用户名冲突) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    auto* repo = mock.get();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_CALL(*repo, existsByEmail(_)).WillOnce(Return(false));
    EXPECT_CALL(*repo, findByUsername(_)).WillOnce(Return(std::optional<UserAuthInfo>(UserAuthInfo{})));

    try {
        service.registerUser(makeRegisterReq());
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 409);
        EXPECT_STREQ(e.what(), "用户名已存在");
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

// ==================== 未实现的方法 ====================

TEST(UserServiceTest, 未实现方法返回501) {
    auto mock = std::make_unique<NiceMock<MockUserRepository>>();
    UserServiceImpl service(std::move(mock), TEST_JWT_SECRET);

    EXPECT_THROW(service.requestPasswordReset("a@b.com"), ServiceException);
    EXPECT_THROW(service.resetPassword("tok", "pw"), ServiceException);
    EXPECT_THROW(service.changePassword(1, "old", "new"), ServiceException);
    EXPECT_THROW(service.updateHealthProfile(1, {}), ServiceException);
    EXPECT_THROW(service.getFavorites(1, 1, 20, ""), ServiceException);
    EXPECT_THROW(service.getFavoriteGroups(1), ServiceException);
    EXPECT_THROW(service.createFavoriteGroup(1, {}), ServiceException);
    EXPECT_THROW(service.updateFavoriteGroup(1, 1, {}), ServiceException);
    EXPECT_THROW(service.deleteFavoriteGroup(1, 1), ServiceException);
    EXPECT_THROW(service.updateFavoriteItem(1, 1, {}), ServiceException);
    EXPECT_THROW(service.batchDeleteFavorites(1, {}), ServiceException);
    EXPECT_THROW(service.getNotifications(1, 1, 20, ""), ServiceException);
    EXPECT_THROW(service.markNotificationRead(1, 1), ServiceException);
    EXPECT_THROW(service.markAllNotificationsRead(1), ServiceException);
    EXPECT_THROW(service.deleteNotification(1, 1), ServiceException);
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

    auto currentProfile = makeUserProfile(42);  // has email "test@example.com"
    auto updatedProfile = makeUserProfile(42);
    updatedProfile.display_name = "新昵称";
    updatedProfile.email = "new@example.com";
    updatedProfile.phone = "13900139000";

    // First findById is for email check (returns current), second is for return value (returns updated)
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
    profile.email = "same@example.com";  // same as current — should NOT call existsByEmail

    EXPECT_CALL(*repo, findById(42))
        .Times(2)
        .WillRepeatedly(Return(current));
    EXPECT_CALL(*repo, existsByEmail(_)).Times(0);  // should not check
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
    expectedResp.avatar_id = 42;
    expectedResp.avatar_url = "/uploads/avatars/user_42_12345.jpg";

    EXPECT_CALL(*repo, uploadAvatar(42, "/tmp/test_avatar.jpg"))
        .WillOnce(Return(expectedResp));

    auto result = service.uploadAvatar(42, "/tmp/test_avatar.jpg");
    EXPECT_EQ(result.avatar_id, 42);
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
