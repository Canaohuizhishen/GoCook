#pragma once

#include <gocook/IAdminRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 管理员仓库的 PostgreSQL 实现，承载 IAdminRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 AdminServiceImpl 调用。
 */
class PgAdminRepository : public gocook::repository::IAdminRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgAdminRepository(ConnectionPool& db) : db_(db) {}

    // 查询用户列表（分页，可带筛选条件）
    gocook::models::PagedUsers findUsers(int page, int size,
                                         const nlohmann::json& filters) override;
    // 创建用户账户
    void createUser(const gocook::models::CreateUserRequest& userData) override;
    // 修改用户信息
    void updateUser(int userId,
                    const gocook::models::UpdateUserRequest& updates) override;
    // 冻结/解封用户
    void setUserStatus(
        int userId, const gocook::models::SetUserStatusRequest& req) override;
    // 删除用户
    void deleteUser(int userId) override;

    // 查询待审核菜谱列表（分页）
    gocook::models::PagedPendingRecipes findPendingRecipes(int page,
                                                           int size) override;
    // 审核通过菜谱
    void approveRecipe(int recipeId) override;
    // 审核拒绝菜谱（携带拒绝原因）
    void rejectRecipe(int recipeId,
                      const gocook::models::RejectRecipeRequest& req) override;
    // 批量审核菜谱，返回成功/失败统计
    gocook::models::BatchReviewResponse batchReviewRecipes(
        const gocook::models::BatchReviewRequest& req) override;

    // 发布系统公告
    void publishAnnouncement(
        const gocook::models::AnnouncementRequest& req) override;
    // 发送系统通知
    gocook::models::NotificationResponse sendNotification(
        const gocook::models::NotificationRequest& notification) override;

    // 查询运营数据统计
    gocook::models::StatisticsData findStatistics() override;
    // 查询管理员操作日志（分页，可按类型/操作者筛选）
    gocook::models::PagedAdminLogs findAdminLogs(int page, int size,
                                                 const std::string& type,
                                                 int userId) override;
    // 查询全局用户行为日志（分页，可按用户/行为筛选）
    gocook::models::PagedActivityLogs findActivityLogs(int page, int size,
                                                       int userId,
                                                       const std::string& action) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
