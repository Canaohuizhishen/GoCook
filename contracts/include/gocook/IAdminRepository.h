#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <string>

namespace gocook::repository {

class IAdminRepository {
public:
    virtual ~IAdminRepository() = default;

    virtual models::PagedUsers findUsers(int page, int size,
                                         const nlohmann::json& filters) = 0;
    virtual void createUser(const models::CreateUserRequest& userData) = 0;
    virtual void updateUser(int userId,
                            const models::UpdateUserRequest& updates) = 0;
    virtual void setUserStatus(
        int userId, const models::SetUserStatusRequest& req) = 0;
    virtual void deleteUser(int userId) = 0;

    virtual models::PagedPendingRecipes findPendingRecipes(int page,
                                                           int size) = 0;
    virtual void approveRecipe(int recipeId) = 0;
    virtual void rejectRecipe(int recipeId,
                              const models::RejectRecipeRequest& req) = 0;
    virtual models::BatchReviewResponse batchReviewRecipes(
        const models::BatchReviewRequest& req) = 0;

    virtual void publishAnnouncement(
        const models::AnnouncementRequest& req) = 0;
    virtual models::NotificationResponse sendNotification(
        const models::NotificationRequest& notification) = 0;

    virtual models::StatisticsData findStatistics() = 0;
    virtual models::PagedAdminLogs findAdminLogs(int page, int size,
                                                 const std::string& type,
                                                 int userId) = 0;
    virtual models::PagedActivityLogs findActivityLogs(int page, int size,
                                                       int userId,
                                                       const std::string& action) = 0;
};

} // namespace gocook::repository
