#pragma once

#include <gocook/IServices.h>
#include "../ConnectionPool.h"
#include <string>

class UserServiceImpl : public gocook::services::IUserService {
public:
    /**
     * @brief 构造函数，注入连接池与 JWT 签名密钥
     * @param db        数据库连接池引用
     * @param jwtSecret JWT HS256 签名密钥，需与 AuthMiddleware 使用相同密钥
     */
    explicit UserServiceImpl(ConnectionPool& db, const std::string& jwtSecret)
        : db_(db), jwt_secret_(jwtSecret) {}

    // 已实现的核心方法
    void registerUser(const gocook::models::RegisterRequest& request) override;
    gocook::models::LoginResponse login(const gocook::models::LoginRequest& request) override;

    // 以下方法暂时未实现（骨架）
    void requestPasswordReset(const std::string& email) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void resetPassword(const std::string& token,
                       const std::string& newPassword) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::UserProfile getCurrentUser(int userId) override;
    gocook::models::UserProfile updateProfile(int userId,
                                              const gocook::models::UpdateProfileRequest& profile) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void changePassword(int userId,
                        const std::string& currentPassword,
                        const std::string& newPassword) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void deleteAccount(int userId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::AvatarUploadResponse uploadAvatar(int userId,
                                                      const std::string& filePath) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::UserPreferences getPreferences(int userId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& healthProfile) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group = "") override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    std::vector<gocook::models::FavoriteGroup> getFavoriteGroups(int userId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::FavoriteGroup createFavoriteGroup(int userId,
                                                      const gocook::models::CreateGroupRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void updateFavoriteGroup(int userId, int groupId,
                             const gocook::models::UpdateGroupRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void deleteFavoriteGroup(int userId, int groupId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void updateFavoriteItem(int userId, int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void batchDeleteFavorites(int userId,
                              const gocook::models::BatchDeleteFavoritesRequest& request) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::PagedNotifications getNotifications(int userId, int page, int size,
                                                        const std::string& type = "") override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void markNotificationRead(int userId, int notificationId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void markAllNotificationsRead(int userId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    void deleteNotification(int userId, int notificationId) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

private:
    ConnectionPool& db_;

    // 生成 JWT Token
    std::string generateToken(int userId, const std::string& username, const std::string& role);

    // 使用 bcrypt 对密码进行哈希（返回完整的 bcrypt 哈希串，包含盐）
    std::string hashPassword(const std::string& plain);

    // 使用 bcrypt 验证明文密码与哈希值是否匹配
    bool validatePassword(const std::string& plain, const std::string& hash);

    // JWT 签名密钥
    std::string jwt_secret_;
};