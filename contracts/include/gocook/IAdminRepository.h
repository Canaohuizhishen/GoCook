#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <string>

namespace gocook::repository {

/**
 * @brief 管理员数据访问抽象接口，定义后台管理域的全部持久化操作。
 *
 * 由 server/repositories/PgAdminRepository 实现（PostgreSQL），
 * 供 AdminServiceImpl 依赖注入调用。
 */
class IAdminRepository {
public:
    virtual ~IAdminRepository() = default;

    /**
     * @brief 查询用户列表（分页）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param filters 筛选条件（JSON 对象，如角色/状态/关键字）
     * @return 分页的管理端用户列表
     */
    virtual models::PagedUsers findUsers(int page, int size,
                                         const nlohmann::json& filters) = 0;

    /**
     * @brief 创建用户账户（管理员）。
     * @param userData 新用户数据
     */
    virtual void createUser(const models::CreateUserRequest& userData) = 0;

    /**
     * @brief 修改用户信息（管理员）。
     * @param userId 用户 ID
     * @param updates 待更新的字段
     */
    virtual void updateUser(int userId,
                            const models::UpdateUserRequest& updates) = 0;

    /**
     * @brief 冻结/解封用户（管理员）。
     * @param userId 用户 ID
     * @param req 目标状态："active" 或 "frozen"
     */
    virtual void setUserStatus(
        int userId, const models::SetUserStatusRequest& req) = 0;

    /**
     * @brief 删除用户（管理员）。
     * @param userId 用户 ID
     */
    virtual void deleteUser(int userId) = 0;

    /**
     * @brief 查询待审核菜谱列表（分页）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的待审核菜谱列表
     */
    virtual models::PagedPendingRecipes findPendingRecipes(int page,
                                                           int size) = 0;

    /**
     * @brief 审核通过菜谱。
     * @param recipeId 菜谱 ID
     */
    virtual void approveRecipe(int recipeId) = 0;

    /**
     * @brief 审核拒绝菜谱。
     * @param recipeId 菜谱 ID
     * @param req 拒绝原因
     */
    virtual void rejectRecipe(int recipeId,
                              const models::RejectRecipeRequest& req) = 0;

    /**
     * @brief 批量审核菜谱。
     * @param req 待审核菜谱 ID 列表与动作（approve/reject）
     * @return 处理结果（成功数量与失败 ID 列表）
     */
    virtual models::BatchReviewResponse batchReviewRecipes(
        const models::BatchReviewRequest& req) = 0;

    /**
     * @brief 发布系统公告。
     * @param req 公告标题与内容
     */
    virtual void publishAnnouncement(
        const models::AnnouncementRequest& req) = 0;

    /**
     * @brief 发送系统通知。
     * @param notification 通知内容与目标
     * @return 通知发送响应（ID/状态/预计接收人数）
     */
    virtual models::NotificationResponse sendNotification(
        const models::NotificationRequest& notification) = 0;

    /**
     * @brief 查询运营数据统计。
     * @return 统计快照（用户/菜谱/审核/增长等）
     */
    virtual models::StatisticsData findStatistics() = 0;

    /**
     * @brief 查询管理员操作日志（分页）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param type 可选日志类型筛选，空串表示全部
     * @param userId 可选操作者 ID 筛选，0 表示全部
     * @return 分页的管理员操作日志
     */
    virtual models::PagedAdminLogs findAdminLogs(int page, int size,
                                                 const std::string& type,
                                                 int userId) = 0;

    /**
     * @brief 查询全局用户行为日志（分页）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param userId 可选用户 ID 筛选，0 表示全部
     * @param action 可选行为类型筛选，空串表示全部
     * @return 分页的用户行为日志
     */
    virtual models::PagedActivityLogs findActivityLogs(int page, int size,
                                                       int userId,
                                                       const std::string& action) = 0;
};

} // namespace gocook::repository
