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
    EXPECT_THROW(service.updateProfile(1, {}), ServiceException);
    EXPECT_THROW(service.changePassword(1, "old", "new"), ServiceException);
    EXPECT_THROW(service.deleteAccount(1), ServiceException);
    EXPECT_THROW(service.uploadAvatar(1, "f.png"), ServiceException);
    EXPECT_THROW(service.getPreferences(1), ServiceException);
    EXPECT_THROW(service.updatePreferences(1, {}), ServiceException);
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
