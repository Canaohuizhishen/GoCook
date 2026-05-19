#pragma once

#include <gocook/IAdminRepository.h>
#include "../common/ConnectionPool.h"

class PgAdminRepository : public gocook::repository::IAdminRepository {
public:
    explicit PgAdminRepository(ConnectionPool& db) : db_(db) {}

    gocook::models::PagedUsers findUsers(int page, int size,
                                         const nlohmann::json& filters) override;
    void createUser(const gocook::models::CreateUserRequest& userData) override;
    void updateUser(int userId,
                    const gocook::models::UpdateUserRequest& updates) override;
    void setUserStatus(
        int userId, const gocook::models::SetUserStatusRequest& req) override;
    void deleteUser(int userId) override;

    gocook::models::PagedPendingRecipes findPendingRecipes(int page,
                                                           int size) override;
    void approveRecipe(int recipeId) override;
    void rejectRecipe(int recipeId,
                      const gocook::models::RejectRecipeRequest& req) override;
    gocook::models::BatchReviewResponse batchReviewRecipes(
        const gocook::models::BatchReviewRequest& req) override;

    void publishAnnouncement(
        const gocook::models::AnnouncementRequest& req) override;
    gocook::models::NotificationResponse sendNotification(
        const gocook::models::NotificationRequest& notification) override;

    gocook::models::StatisticsData findStatistics() override;
    gocook::models::PagedAdminLogs findAdminLogs(int page, int size,
                                                 const std::string& type,
                                                 int userId) override;
    gocook::models::PagedActivityLogs findActivityLogs(int page, int size,
                                                       int userId,
                                                       const std::string& action) override;

private:
    ConnectionPool& db_;
};
