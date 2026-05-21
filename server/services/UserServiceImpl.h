#pragma once

#include <gocook/IServices.h>
#include <gocook/IUserRepository.h>
#include <string>
#include <memory>

class UserServiceImpl : public gocook::services::IUserService {
public:
    /**
     * @brief 构造函数，注入用户仓库与 JWT 签名密钥
     * @param userRepo  用户仓库抽象指针
     * @param jwtSecret JWT HS256 签名密钥，需与 AuthMiddleware 使用相同密钥
     */
    explicit UserServiceImpl(std::unique_ptr<gocook::repository::IUserRepository> userRepo,
                             const std::string& jwtSecret)
        : userRepo_(std::move(userRepo)), jwt_secret_(jwtSecret) {}

    void registerUser(const gocook::models::RegisterRequest& request) override;
    gocook::models::LoginResponse login(const gocook::models::LoginRequest& request) override;

    void requestPasswordReset(const std::string& email) override;
    void resetPassword(const std::string& token,
                       const std::string& newPassword) override;
    gocook::models::UserProfile getCurrentUser(int userId) override;
    gocook::models::UserProfile updateProfile(int userId,
                                               const gocook::models::UpdateProfileRequest& profile) override;
    void changePassword(int userId,
                        const std::string& currentPassword,
                        const std::string& newPassword) override;
    void deleteAccount(int userId) override;
    gocook::models::AvatarUploadResponse uploadAvatar(int userId,
                                                       const std::string& filePath) override;
    gocook::models::UserPreferences getPreferences(int userId) override;
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override;
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& healthProfile) override;
    gocook::models::HealthProfileResponse getHealthProfile(int userId) override;
    gocook::models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group = "") override;
    std::vector<gocook::models::FavoriteGroup> getFavoriteGroups(int userId) override;
    gocook::models::FavoriteGroup createFavoriteGroup(int userId,
                                                       const gocook::models::CreateGroupRequest& request) override;
    void updateFavoriteGroup(int userId, int groupId,
                             const gocook::models::UpdateGroupRequest& request) override;
    void deleteFavoriteGroup(int userId, int groupId) override;
    void updateFavoriteItem(int userId, int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& request) override;
    void batchDeleteFavorites(int userId,
                              const gocook::models::BatchDeleteFavoritesRequest& request) override;
    gocook::models::PagedNotifications getNotifications(int userId, int page, int size,
                                                        const std::string& type = "") override;
    void markNotificationRead(int userId, int notificationId) override;
    void markAllNotificationsRead(int userId) override;
    void deleteNotification(int userId, int notificationId) override;

private:
    std::unique_ptr<gocook::repository::IUserRepository> userRepo_;

    std::string generateToken(int userId, const std::string& username, const std::string& role);
    std::string hashPassword(const std::string& plain);
    bool validatePassword(const std::string& plain, const std::string& hash);

    std::string jwt_secret_;
};
