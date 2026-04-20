#pragma once

#include <gocook/IServices.h>   // IAdminService 接口定义
#include "../DBConnection.h"

class AdminServiceImpl : public gocook::services::IAdminService {
public:
    explicit AdminServiceImpl(DBConnection& db) : db_(db) {}

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

private:
    DBConnection& db_;
};