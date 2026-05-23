#pragma once

#include <gocook/DataModels.h>
#include <string>
#include <optional>
#include <vector>

namespace gocook::repository {

struct UserAuthInfo {
    int id = 0;
    std::string username;
    std::string passwordHash;
    std::string role;
};

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<UserAuthInfo> findByUsername(const std::string& username) = 0;
    virtual bool existsByEmail(const std::string& email) = 0;
    virtual void createUser(const std::string& username,
                            const std::string& passwordHash,
                            const std::string& email) = 0;

    virtual std::optional<models::UserProfile> findById(int userId) = 0;
    /// 通过邮箱查找用户 ID（用于忘记密码）
    virtual std::optional<int> findIdByEmail(const std::string& email) = 0;
    /// 通过用户名+邮箱联合查找用户 ID（用于忘记密码验证身份）
    virtual std::optional<int> findIdByUsernameAndEmail(const std::string& username,
                                                        const std::string& email) = 0;

    /// 创建密码重置令牌
    virtual void createPasswordResetToken(int userId, const std::string& token,
                                          const std::string& expiresAt) = 0;
    /// 根据令牌查找对应用户ID（仅返回未使用且未过期的令牌）
    virtual std::optional<int> findUserIdByResetToken(const std::string& token) = 0;
    /// 标记密码重置令牌为已使用
    virtual void markResetTokenUsed(const std::string& token) = 0;
    virtual void updateProfile(int userId,
                               const models::UpdateProfileRequest& profile) = 0;
    /// 获取用户密码哈希（用于修改密码时验证原密码）
    virtual std::string getPasswordHash(int userId) = 0;
    virtual void changePassword(int userId,
                                const std::string& newPasswordHash) = 0;
    virtual void deleteAccount(int userId) = 0;

    virtual models::AvatarUploadResponse uploadAvatar(
        int userId, const std::string& filePath) = 0;

    virtual models::UserPreferences getPreferences(int userId) = 0;
    virtual void updatePreferences(int userId,
                                   const models::UserPreferences& prefs) = 0;
    virtual std::vector<std::string> getHealthConditions(int userId) = 0;
    virtual models::HealthProfileResponse updateHealthProfile(
        int userId, const models::HealthProfileRequest& req) = 0;
    virtual models::HealthProfileResponse getHealthProfile(int userId) = 0;

    virtual models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group) = 0;
    virtual std::vector<models::FavoriteGroup> getFavoriteGroups(int userId) = 0;
    virtual models::FavoriteGroup createFavoriteGroup(
        int userId, const models::CreateGroupRequest& req) = 0;
    virtual void updateFavoriteGroup(int userId, int groupId,
                                     const models::UpdateGroupRequest& req) = 0;
    virtual void deleteFavoriteGroup(int userId, int groupId) = 0;
    virtual void updateFavoriteItem(int userId, int favoriteId,
                                    const models::UpdateFavoriteRequest& req) = 0;
    virtual void batchDeleteFavorites(
        int userId, const models::BatchDeleteFavoritesRequest& req) = 0;

    virtual models::PagedNotifications getNotifications(int userId, int page,
                                                        int size,
                                                        const std::string& type) = 0;
    virtual void markNotificationRead(int userId, int notificationId) = 0;
    virtual void markAllNotificationsRead(int userId) = 0;
    virtual void deleteNotification(int userId, int notificationId) = 0;
};

} // namespace gocook::repository
