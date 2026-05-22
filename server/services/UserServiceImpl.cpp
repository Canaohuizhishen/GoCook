#include "UserServiceImpl.h"
#include <jwt-cpp/jwt.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <cctype>
#include "../common/Logger.h"
#include "../common/EmailSender.h"
#include "bcrypt/crypt_blowfish.h"
#include <openssl/crypto.h>

#define BCRYPT_OUTPUT_SIZE 128

using namespace gocook::models;
using namespace gocook::services;

namespace {
    void populateAvoidanceSuggestions(
        const std::vector<std::string>& conditions,
        std::vector<AvoidanceItem>& out)
    {
        for (const auto& c : conditions) {
            if (c == "高血压") {
                out.push_back({"高钠食物", "高血压患者应限制钠摄入"});
                out.push_back({"动物内脏", "含较高胆固醇，不利于血压控制"});
                out.push_back({"腌制食品", "含盐量高，可能导致血压升高"});
            } else if (c == "高血脂") {
                out.push_back({"油炸食品", "高脂肪含量，不利于血脂控制"});
                out.push_back({"肥肉", "饱和脂肪酸含量高"});
                out.push_back({"动物内脏", "胆固醇含量较高"});
            } else if (c == "糖尿病") {
                out.push_back({"高糖食品", "含添加糖，不利于血糖控制"});
                out.push_back({"精制米面", "升糖指数高，建议选择全谷物"});
                out.push_back({"含糖饮料", "高糖饮品，应避免"});
            } else if (c == "胃炎") {
                out.push_back({"辛辣食物", "刺激胃黏膜，可能加重炎症"});
                out.push_back({"生冷食物", "不易消化，增加胃负担"});
                out.push_back({"酒精", "刺激胃黏膜，应避免饮酒"});
            } else if (c == "痛风") {
                out.push_back({"高嘌呤食物", "如动物内脏、浓汤等"});
                out.push_back({"海鲜", "嘌呤含量较高"});
                out.push_back({"啤酒", "影响尿酸排泄"});
            }
        }
    }
}

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

// ==================== 密码重置 ====================

void UserServiceImpl::requestPasswordReset(const std::string& username,
                                              const std::string& email) {
    // 1. Verify username + email match (双重验证)
    auto userIdOpt = userRepo_->findIdByUsernameAndEmail(username, email);
    if (!userIdOpt.has_value()) {
        LOG_WARN("Password reset requested for username='%s' email='%s' — no match found",
                 username.c_str(), email.c_str());
        throw ServiceException("用户名和邮箱不匹配", 400);
    }

    // 2. Generate random token (64 hex chars = 256 bits)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 15);
    std::ostringstream tokenStream;
    for (int i = 0; i < 64; ++i) {
        tokenStream << std::hex << distrib(gen);
    }
    std::string token = tokenStream.str();

    // 3. Set expiry to 1 hour from now (ISO 8601 format for PostgreSQL timestamp)
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(1);
    auto expTt = std::chrono::system_clock::to_time_t(exp);
    std::ostringstream expStream;
    expStream << std::put_time(std::gmtime(&expTt), "%Y-%m-%dT%H:%M:%SZ");
    std::string expiresAt = expStream.str();

    // 4. Store token in database
    userRepo_->createPasswordResetToken(userIdOpt.value(), token, expiresAt);

    // 5. Send email (fallback to log if SMTP not configured)
    bool sent = EmailSender::sendPasswordResetEmail(email, token);
    if (!sent) {
        LOG_WARN("SMTP not configured for %s, token logged to stderr", email.c_str());
        std::cerr << "\n*** PASSWORD RESET TOKEN ***" << std::endl;
        std::cerr << "Email: " << email << std::endl;
        std::cerr << "Token: " << token << std::endl;
        std::cerr << "Expires: " << expiresAt << std::endl;
        std::cerr << "To reset: POST /api/password/reset with {token, new_password}" << std::endl;
        std::cerr << "*** END TOKEN ***\n" << std::endl;
    } else {
        LOG_INFO("Password reset email sent to %s", email.c_str());
    }
}

void UserServiceImpl::resetPassword(const std::string& token, const std::string& newPassword) {
    // 1. Validate token — find user_id from valid (unused & not expired) token
    auto userIdOpt = userRepo_->findUserIdByResetToken(token);
    if (!userIdOpt.has_value()) {
        throw ServiceException("令牌无效或已过期", 400);
    }

    // 2. Validate new password strength (reuse same rules as changePassword)
    if (newPassword.size() < 6) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }
    bool hasLetter = false, hasDigit = false;
    for (char c : newPassword) {
        if (std::isalpha(static_cast<unsigned char>(c))) hasLetter = true;
        if (std::isdigit(static_cast<unsigned char>(c))) hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }

    // 3. Hash new password
    std::string newHash = hashPassword(newPassword);

    // 4. Update password
    userRepo_->changePassword(userIdOpt.value(), newHash);

    // 5. Mark token as used (one-time use)
    userRepo_->markResetTokenUsed(token);
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
void UserServiceImpl::changePassword(int userId,
                                      const std::string& currentPassword,
                                      const std::string& newPassword) {
    // 1. 获取当前密码哈希
    std::string currentHash = userRepo_->getPasswordHash(userId);

    // 2. 验证原密码
    if (!validatePassword(currentPassword, currentHash)) {
        throw ServiceException("原密码不正确", 400);
    }

    // 3. 验证新密码强度（至少6位，含字母和数字）
    if (newPassword.size() < 6) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }
    bool hasLetter = false, hasDigit = false;
    for (char c : newPassword) {
        if (std::isalpha(static_cast<unsigned char>(c))) hasLetter = true;
        if (std::isdigit(static_cast<unsigned char>(c))) hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }

    // 4. 对新密码进行哈希
    std::string newHash = hashPassword(newPassword);

    // 5. 更新数据库
    userRepo_->changePassword(userId, newHash);
}
void UserServiceImpl::deleteAccount(int userId) {
    // Verify user exists first
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    userRepo_->deleteAccount(userId);
}
AvatarUploadResponse UserServiceImpl::uploadAvatar(int userId, const std::string& filePath) {
    if (filePath.empty()) {
        throw ServiceException("文件路径无效", 400);
    }
    return userRepo_->uploadAvatar(userId, filePath);
}
UserPreferences UserServiceImpl::getPreferences(int userId) {
    // Verify user exists first
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return userRepo_->getPreferences(userId);
}
void UserServiceImpl::updatePreferences(int userId, const UserPreferences& prefs) {
    // Verify user exists first
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    userRepo_->updatePreferences(userId, prefs);
}
HealthProfileResponse UserServiceImpl::updateHealthProfile(int userId, const HealthProfileRequest& req) {
    // Verify user exists first
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }

    // Save health profile to database
    userRepo_->updateHealthProfile(userId, req);

    // Generate avoidance suggestions based on user's health conditions
    HealthProfileResponse resp;
    // Populate saved fields in case the caller needs them
    resp.height_cm = req.height_cm;
    resp.weight_kg = req.weight_kg;
    resp.conditions = req.conditions;
    // Populate avoidance suggestions
    populateAvoidanceSuggestions(req.conditions, resp.suggested_avoidances);
    return resp;
}

HealthProfileResponse UserServiceImpl::getHealthProfile(int userId) {
    // Verify user exists
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }

    // Load health profile from database
    auto resp = userRepo_->getHealthProfile(userId);

    // Generate avoidance suggestions based on saved conditions
    populateAvoidanceSuggestions(resp.conditions, resp.suggested_avoidances);

    return resp;
}

PagedFavorites UserServiceImpl::getFavorites(int userId, int page, int size, const std::string& group) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getFavorites(userId, page, size, group);
}
std::vector<FavoriteGroup> UserServiceImpl::getFavoriteGroups(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getFavoriteGroups(userId);
}
FavoriteGroup UserServiceImpl::createFavoriteGroup(int userId, const CreateGroupRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->createFavoriteGroup(userId, req);
}
void UserServiceImpl::updateFavoriteGroup(int userId, int groupId, const UpdateGroupRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->updateFavoriteGroup(userId, groupId, req);
}
void UserServiceImpl::deleteFavoriteGroup(int userId, int groupId) {
    userRepo_->deleteFavoriteGroup(userId, groupId);
}
void UserServiceImpl::updateFavoriteItem(int userId, int favoriteId, const UpdateFavoriteRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->updateFavoriteItem(userId, favoriteId, req);
}
void UserServiceImpl::batchDeleteFavorites(int userId, const BatchDeleteFavoritesRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->batchDeleteFavorites(userId, req);
}
PagedNotifications UserServiceImpl::getNotifications(int userId, int page, int size, const std::string& type) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getNotifications(userId, page, size, type);
}
void UserServiceImpl::markNotificationRead(int userId, int notificationId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->markNotificationRead(userId, notificationId);
}
void UserServiceImpl::markAllNotificationsRead(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->markAllNotificationsRead(userId);
}
void UserServiceImpl::deleteNotification(int userId, int notificationId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->deleteNotification(userId, notificationId);
}
