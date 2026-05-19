#pragma once

#include <gocook/IUserRepository.h>
#include <gmock/gmock.h>

class MockUserRepository : public gocook::repository::IUserRepository {
public:
    MOCK_METHOD(std::optional<gocook::repository::UserAuthInfo>, findByUsername,
                (const std::string&), (override));
    MOCK_METHOD(bool, existsByEmail, (const std::string&), (override));
    MOCK_METHOD(void, createUser,
                (const std::string&, const std::string&, const std::string&), (override));
    MOCK_METHOD(std::optional<gocook::models::UserProfile>, findById, (int), (override));
    MOCK_METHOD(void, updateProfile,
                (int, const gocook::models::UpdateProfileRequest&), (override));
    MOCK_METHOD(void, changePassword, (int, const std::string&), (override));
    MOCK_METHOD(void, deleteAccount, (int), (override));
    MOCK_METHOD(std::string, getPasswordHash, (int), (override));
    MOCK_METHOD(gocook::models::AvatarUploadResponse, uploadAvatar,
                (int, const std::string&), (override));
    MOCK_METHOD(gocook::models::UserPreferences, getPreferences, (int), (override));
    MOCK_METHOD(void, updatePreferences,
                (int, const gocook::models::UserPreferences&), (override));
    MOCK_METHOD(gocook::models::HealthProfileResponse, updateHealthProfile,
                (int, const gocook::models::HealthProfileRequest&), (override));
    MOCK_METHOD(gocook::models::PagedFavorites, getFavorites,
                (int, int, int, const std::string&), (override));
    MOCK_METHOD(std::vector<gocook::models::FavoriteGroup>, getFavoriteGroups,
                (int), (override));
    MOCK_METHOD(gocook::models::FavoriteGroup, createFavoriteGroup,
                (int, const gocook::models::CreateGroupRequest&), (override));
    MOCK_METHOD(void, updateFavoriteGroup,
                (int, int, const gocook::models::UpdateGroupRequest&), (override));
    MOCK_METHOD(void, deleteFavoriteGroup, (int, int), (override));
    MOCK_METHOD(void, updateFavoriteItem,
                (int, int, const gocook::models::UpdateFavoriteRequest&), (override));
    MOCK_METHOD(void, batchDeleteFavorites,
                (int, const gocook::models::BatchDeleteFavoritesRequest&), (override));
    MOCK_METHOD(gocook::models::PagedNotifications, getNotifications,
                (int, int, int, const std::string&), (override));
    MOCK_METHOD(void, markNotificationRead, (int, int), (override));
    MOCK_METHOD(void, markAllNotificationsRead, (int), (override));
    MOCK_METHOD(void, deleteNotification, (int, int), (override));
};
