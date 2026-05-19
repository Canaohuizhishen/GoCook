#include "UserServiceImpl.h"
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include "../common/Logger.h"
#include "bcrypt/crypt_blowfish.h"
#include <openssl/crypto.h>

#define BCRYPT_OUTPUT_SIZE 128

using namespace gocook::models;
using namespace gocook::services;

std::string UserServiceImpl::generateToken(int userId, const std::string& username, const std::string& role) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 7);

    auto token = jwt::create()
                     .set_issuer("GoCook")
                     .set_type("JWS")
                     .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
                     .set_payload_claim("username", jwt::claim(username))
                     .set_payload_claim("role", jwt::claim(role))
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .sign(jwt::algorithm::hs256{jwt_secret_});

    return token;
}

std::string UserServiceImpl::hashPassword(const std::string& plain) {
    char random_bytes[16];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for (int i = 0; i < 16; ++i) {
        random_bytes[i] = static_cast<char>(distrib(gen));
    }

    char salt[BCRYPT_OUTPUT_SIZE];
    char *salt_result = _crypt_gensalt_blowfish_rn("$2a$", 10, random_bytes, 16, salt, sizeof(salt));
    if (!salt_result) {
        throw ServiceException("Failed to generate password salt");
    }

    char hash[BCRYPT_OUTPUT_SIZE];
    char *hash_result = _crypt_blowfish_rn(plain.c_str(), salt, hash, sizeof(hash));
    if (!hash_result) {
        throw ServiceException("Failed to hash password");
    }

    return std::string(hash_result);
}

bool UserServiceImpl::validatePassword(const std::string& plain, const std::string& hash) {
    char output[BCRYPT_OUTPUT_SIZE];
    char *result = _crypt_blowfish_rn(plain.c_str(), hash.c_str(), output, sizeof(output));
    if (!result) {
        return false;
    }

    size_t result_len = std::strlen(result);
    if (hash.size() != result_len) {
        return false;
    }

    return CRYPTO_memcmp(hash.data(), result, result_len) == 0;
}

void UserServiceImpl::registerUser(const RegisterRequest& request) {
    if (userRepo_->existsByEmail(request.email)) {
        throw ServiceException("邮箱已被注册", 409);
    }

    auto existing = userRepo_->findByUsername(request.username);
    if (existing.has_value()) {
        throw ServiceException("用户名已存在", 409);
    }

    std::string hashed = hashPassword(request.password);
    userRepo_->createUser(request.username, hashed, request.email);
}

LoginResponse UserServiceImpl::login(const LoginRequest& request) {
    auto authInfo = userRepo_->findByUsername(request.username);
    if (!authInfo.has_value()) {
        throw ServiceException("Invalid username or password", 401);
    }

    if (!validatePassword(request.password, authInfo->passwordHash)) {
        throw ServiceException("Invalid username or password", 401);
    }

    LoginResponse resp;
    resp.user_id = authInfo->id;
    resp.username = authInfo->username;
    resp.token = generateToken(authInfo->id, authInfo->username, authInfo->role);
    return resp;
}

UserProfile UserServiceImpl::getCurrentUser(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return *user;
}

// 以下方法暂时未实现（骨架）- updateProfile 已实现，uploadAvatar 已实现
void UserServiceImpl::requestPasswordReset(const std::string&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::resetPassword(const std::string&, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
UserProfile UserServiceImpl::updateProfile(int userId, const UpdateProfileRequest& profile) {
    // Validate: at least one field must be provided
    if (!profile.display_name.has_value() && !profile.avatar_url.has_value() &&
        !profile.avatar_id.has_value() && !profile.email.has_value() &&
        !profile.phone.has_value()) {
        throw ServiceException("没有提供需要更新的字段", 400);
    }

    // Validate display_name not empty
    if (profile.display_name.has_value() && profile.display_name->empty()) {
        throw ServiceException("昵称不能为空", 400);
    }

    // Resolve avatar_id → if avatar_id equals userId, keep current avatar_url
    // (the upload already set it). If avatar_id is set but different, ignore it.
    UpdateProfileRequest resolvedProfile = profile;
    if (profile.avatar_id.has_value() && profile.avatar_id.value() == userId) {
        // Keep existing avatar_url — set resolvedProfile.avatar_url to nullopt
        // so the repo's COALESCE won't override it
        resolvedProfile.avatar_url = std::nullopt;
    }

    // Check email uniqueness if being changed
    if (profile.email.has_value() && !profile.email->empty()) {
        auto currentUser = userRepo_->findById(userId);
        if (currentUser.has_value() && profile.email.value() != currentUser->email) {
            if (userRepo_->existsByEmail(profile.email.value())) {
                throw ServiceException("邮箱已被注册", 409);
            }
        }
    }

    userRepo_->updateProfile(userId, resolvedProfile);

    // Return updated profile
    auto updated = userRepo_->findById(userId);
    if (!updated.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return *updated;
}
void UserServiceImpl::changePassword(int, const std::string&, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::deleteAccount(int) {
    throw ServiceException("Not implemented", 501);
}
AvatarUploadResponse UserServiceImpl::uploadAvatar(int userId, const std::string& filePath) {
    if (filePath.empty()) {
        throw ServiceException("文件路径无效", 400);
    }
    return userRepo_->uploadAvatar(userId, filePath);
}
UserPreferences UserServiceImpl::getPreferences(int) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::updatePreferences(int, const UserPreferences&) {
    throw ServiceException("Not implemented", 501);
}
HealthProfileResponse UserServiceImpl::updateHealthProfile(int, const HealthProfileRequest&) {
    throw ServiceException("Not implemented", 501);
}
PagedFavorites UserServiceImpl::getFavorites(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
std::vector<FavoriteGroup> UserServiceImpl::getFavoriteGroups(int) {
    throw ServiceException("Not implemented", 501);
}
FavoriteGroup UserServiceImpl::createFavoriteGroup(int, const CreateGroupRequest&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::updateFavoriteGroup(int, int, const UpdateGroupRequest&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::deleteFavoriteGroup(int, int) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::updateFavoriteItem(int, int, const UpdateFavoriteRequest&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::batchDeleteFavorites(int, const BatchDeleteFavoritesRequest&) {
    throw ServiceException("Not implemented", 501);
}
PagedNotifications UserServiceImpl::getNotifications(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::markNotificationRead(int, int) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::markAllNotificationsRead(int) {
    throw ServiceException("Not implemented", 501);
}
void UserServiceImpl::deleteNotification(int, int) {
    throw ServiceException("Not implemented", 501);
}
