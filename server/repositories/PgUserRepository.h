#pragma once

#include <gocook/IUserRepository.h>
#include "../common/ConnectionPool.h"

class PgUserRepository : public gocook::repository::IUserRepository {
public:
    explicit PgUserRepository(ConnectionPool& db) : db_(db) {}

    std::optional<gocook::repository::UserAuthInfo> findByUsername(const std::string& username) override;
    bool existsByEmail(const std::string& email) override;
    void createUser(const std::string& username,
                    const std::string& passwordHash,
                    const std::string& email) override;

    std::optional<gocook::models::UserProfile> findById(int userId) override;
    void updateProfile(int userId,
                       const gocook::models::UpdateProfileRequest& profile) override;
    std::string getPasswordHash(int userId) override;
    void changePassword(int userId, const std::string& newPasswordHash) override;
    void deleteAccount(int userId) override;

    gocook::models::AvatarUploadResponse uploadAvatar(
        int userId, const std::string& filePath) override;

    gocook::models::UserPreferences getPreferences(int userId) override;
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override;
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& req) override;
    gocook::models::HealthProfileResponse getHealthProfile(int userId) override;

    gocook::models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group) override;
    std::vector<gocook::models::FavoriteGroup> getFavoriteGroups(int userId) override;
    gocook::models::FavoriteGroup createFavoriteGroup(
        int userId, const gocook::models::CreateGroupRequest& req) override;
    void updateFavoriteGroup(int userId, int groupId,
                             const gocook::models::UpdateGroupRequest& req) override;
    void deleteFavoriteGroup(int userId, int groupId) override;
    void updateFavoriteItem(int userId, int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& req) override;
    void batchDeleteFavorites(
        int userId, const gocook::models::BatchDeleteFavoritesRequest& req) override;

    gocook::models::PagedNotifications getNotifications(int userId, int page,
                                                        int size,
                                                        const std::string& type) override;
    void markNotificationRead(int userId, int notificationId) override;
    void markAllNotificationsRead(int userId) override;
    void deleteNotification(int userId, int notificationId) override;

private:
    ConnectionPool& db_;
};
