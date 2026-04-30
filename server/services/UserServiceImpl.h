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

    // 验证用户名密码（明文比较，后续应改为 bcrypt 验证）
    bool validatePassword(const std::string& plain, const std::string& storedHash);

    // 哈希密码（当前直接返回原字符串，后续替换为 bcrypt）
    std::string hashPassword(const std::string& plain);

    // JWT 签名密钥，生产环境应从安全配置中读取
    const std::string jwt_secret = "GoCook-Project-Secret-Key-Change-Me-In-Production";
};