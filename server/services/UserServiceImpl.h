#pragma once

#include <gocook/IServices.h>
#include "../DBConnection.h"
#include <string>

class UserServiceImpl : public gocook::services::IUserService {
public:
    explicit UserServiceImpl(DBConnection& db) : db_(db) {}

    // 已实现的核心方法
    void registerUser(const gocook::models::RegisterRequest& request) override;
    gocook::models::LoginResponse login(const gocook::models::LoginRequest& request) override;

    // 以下方法暂时未实现（骨架）
    gocook::models::UserProfile getCurrentUser(int userId) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    gocook::models::UserProfile updateProfile(int userId,
                                              const gocook::models::UpdateProfileRequest& profile) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    gocook::models::UserPreferences getPreferences(int userId) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& healthProfile) override {
        throw gocook::services::ServiceException("Not implemented");
    }
    gocook::models::PagedFavorites getFavorites(int userId, int page, int size) override {
        throw gocook::services::ServiceException("Not implemented");
    }

private:
    DBConnection& db_;

    // 生成 JWT Token
    std::string generateToken(int userId, const std::string& username);

    // 使用 bcrypt 对密码进行哈希（返回完整的 bcrypt 哈希串，包含盐）
    std::string hashPassword(const std::string& plain);

    // 使用 bcrypt 验证明文密码与哈希值是否匹配
    bool validatePassword(const std::string& plain, const std::string& hash);

    // JWT 签名密钥，生产环境应从安全配置中读取
    const std::string jwt_secret = "GoCook-Project-Secret-Key-Change-Me-In-Production";
};