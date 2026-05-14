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
    virtual void updateProfile(int userId,
                               const models::UpdateProfileRequest& profile) = 0;
    virtual void changePassword(int userId,
                                const std::string& newPasswordHash) = 0;
    virtual void deleteAccount(int userId) = 0;

    virtual models::AvatarUploadResponse uploadAvatar(
        int userId, const std::string& filePath) = 0;

    virtual models::UserPreferences getPreferences(int userId) = 0;
    virtual void updatePreferences(int userId,
                                   const models::UserPreferences& prefs) = 0;
    virtual models::HealthProfileResponse updateHealthProfile(
        int userId, const models::HealthProfileRequest& req) = 0;

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
