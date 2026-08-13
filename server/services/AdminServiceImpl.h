#pragma once

#include <gocook/IServices.h>
#include <gocook/IAdminRepository.h>
#include <memory>

/**
 * @brief 管理员服务实现，承载 IAdminService 接口定义的全部后台管理业务逻辑。
 *
 * 依赖管理员仓库抽象完成数据访问，由 AdminHandler 调用。
 */
class AdminServiceImpl : public gocook::services::IAdminService {
public:
    // 构造函数，注入管理员仓库抽象
    explicit AdminServiceImpl(std::unique_ptr<gocook::repository::IAdminRepository> adminRepo)
        : adminRepo_(std::move(adminRepo)) {}

    // ---------- 用户管理 ----------
    // 获取用户列表（分页，可带筛选条件）
    gocook::models::PagedUsers getUsers(int page, int size,
                                        const nlohmann::json& filters) override;
    // 创建用户账户
    void createUser(const gocook::models::CreateUserRequest& userData) override;
    // 修改用户信息
    void updateUser(int userId,
                    const gocook::models::UpdateUserRequest& updates) override;
    // 冻结/解封用户
    void setUserStatus(int userId,
                       const gocook::models::SetUserStatusRequest& request) override;
    // 删除用户
    void deleteUser(int userId) override;

    // ---------- 菜谱审核 ----------
    // 获取待审核菜谱列表（分页）
    gocook::models::PagedPendingRecipes getPendingRecipes(int page, int size) override;
    // 审核通过菜谱
    void approveRecipe(int recipeId) override;
    // 审核拒绝菜谱（携带拒绝原因）
    void rejectRecipe(int recipeId,
                      const gocook::models::RejectRecipeRequest& request) override;
    // 批量审核菜谱，返回成功/失败统计
    gocook::models::BatchReviewResponse batchReviewRecipes(
        const gocook::models::BatchReviewRequest& request) override;

    // ---------- 公告与通知 ----------
    // 发布系统公告
    void publishAnnouncement(const gocook::models::AnnouncementRequest& request) override;
    // 发送系统通知
    gocook::models::NotificationResponse sendNotification(
        const gocook::models::NotificationRequest& notification) override;

    // ---------- 统计与日志 ----------
    // 获取运营数据统计（暂未实现，始终抛 501）
    gocook::models::StatisticsData getStatistics() override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    // 查询管理员操作日志（暂未实现，始终抛 501）
    gocook::models::PagedAdminLogs getAdminLogs(int page, int size,
                                                const std::string& type = "",
                                                int userId = 0) override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }
    // 查询全局用户行为日志（暂未实现，始终抛 501）
    gocook::models::PagedActivityLogs getActivityLogs(int page, int size,
                                                      int userId = 0,
                                                      const std::string& action = "") override {
        throw gocook::services::ServiceException("Not implemented", 501);
    }

private:
    std::unique_ptr<gocook::repository::IAdminRepository> adminRepo_; ///< 管理员仓库抽象
};
