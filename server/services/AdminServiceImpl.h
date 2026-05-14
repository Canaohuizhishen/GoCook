#pragma once

#include <gocook/IServices.h>
#include <gocook/IAdminRepository.h>
#include <memory>

class AdminServiceImpl : public gocook::services::IAdminService {
public:
    explicit AdminServiceImpl(std::unique_ptr<gocook::repository::IAdminRepository> adminRepo)
        : adminRepo_(std::move(adminRepo)) {}

    // ---------- 用户管理 ----------
    gocook::models::PagedUsers getUsers(int page, int size,
                                        const nlohmann::json& filters) override;
    void createUser(const gocook::models::CreateUserRequest& userData) override;
    void updateUser(int userId,
                    const gocook::models::UpdateUserRequest& updates) override;
    void setUserStatus(int userId,
                       const gocook::models::SetUserStatusRequest& request) override;
    void deleteUser(int userId) override;

    // ---------- 菜谱审核 ----------
    gocook::models::PagedPendingRecipes getPendingRecipes(int page, int size) override;
    void approveRecipe(int recipeId) override;
    void rejectRecipe(int recipeId,
                      const gocook::models::RejectRecipeRequest& request) override;
    gocook::models::BatchReviewResponse batchReviewRecipes(
        const gocook::models::BatchReviewRequest& request) override;

    // ---------- 公告与通知 ----------
    void publishAnnouncement(const gocook::models::AnnouncementRequest& request) override;
    gocook::models::NotificationResponse sendNotification(
        const gocook::models::NotificationRequest& notification) override;

    // ---------- 统计与日志 ----------
    gocook::models::StatisticsData getStatistics() override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::PagedAdminLogs getAdminLogs(int page, int size,
                                                const std::string& type = "",
                                                int userId = 0) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    gocook::models::PagedActivityLogs getActivityLogs(int page, int size,
                                                      int userId = 0,
                                                      const std::string& action = "") override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

private:
    std::unique_ptr<gocook::repository::IAdminRepository> adminRepo_;
};