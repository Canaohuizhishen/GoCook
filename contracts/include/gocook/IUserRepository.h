#pragma once

#include <gocook/DataModels.h>
#include <string>
#include <optional>
#include <vector>

namespace gocook::repository {

/// 密码重置令牌过期时间（分钟）
inline constexpr int TOKEN_EXPIRY_MINUTES = 15;

/// 用户认证信息（用于登录验证与鉴权）
struct UserAuthInfo {
    int id = 0;                    ///< 用户 ID
    std::string username;          ///< 用户名
    std::string passwordHash;      ///< 密码哈希（不可逆存储）
    std::string role;              ///< 角色：user / moderator / super_admin
};

/**
 * @brief 用户数据访问抽象接口，定义用户域的全部持久化操作。
 *
 * 由 server/repositories/PgUserRepository 实现（PostgreSQL），
 * 供 UserServiceImpl 依赖注入调用。
 */
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    /**
     * @brief 按用户名查询用户认证信息。
     * @param username 用户名
     * @return 认证信息；用户不存在时返回 nullopt
     */
    virtual std::optional<UserAuthInfo> findByUsername(const std::string& username) = 0;

    /**
     * @brief 检查邮箱是否已被注册。
     * @param email 邮箱地址
     * @return true 表示已存在
     */
    virtual bool existsByEmail(const std::string& email) = 0;

    /**
     * @brief 创建新用户。
     * @param username 用户名
     * @param passwordHash 密码哈希
     * @param email 邮箱地址
     */
    virtual void createUser(const std::string& username,
                            const std::string& passwordHash,
                            const std::string& email) = 0;

    /**
     * @brief 按 ID 查询用户公开资料。
     * @param userId 用户 ID
     * @return 用户资料；不存在时返回 nullopt
     */
    virtual std::optional<models::UserProfile> findById(int userId) = 0;

    /// 通过邮箱查找用户 ID（用于忘记密码）
    virtual std::optional<int> findIdByEmail(const std::string& email) = 0;

    /// 通过用户名+邮箱联合查找用户 ID（用于忘记密码验证身份）
    virtual std::optional<int> findIdByUsernameAndEmail(const std::string& username,
                                                        const std::string& email) = 0;

    /// 创建密码重置令牌（过期时间由 SQL 层统一设置为 NOW() + INTERVAL '15 minutes'）
    virtual void createPasswordResetToken(int userId, const std::string& token) = 0;

    /// 根据令牌查找对应用户ID（仅返回未使用且未过期的令牌）
    virtual std::optional<int> findUserIdByResetToken(const std::string& token) = 0;

    /// 标记密码重置令牌为已使用
    virtual void markResetTokenUsed(const std::string& token) = 0;

    /// 在同一事务中重置密码并标记令牌为已用（防止令牌重放）
    virtual void resetPasswordAndMarkTokenUsed(int userId,
                                                const std::string& newPasswordHash,
                                                const std::string& token) = 0;

    /**
     * @brief 更新用户个人资料。
     * @param userId 用户 ID
     * @param profile 待更新的资料字段（可选字段为空表示不修改）
     */
    virtual void updateProfile(int userId,
                               const models::UpdateProfileRequest& profile) = 0;

    /// 获取用户密码哈希（用于修改密码时验证原密码）
    virtual std::string getPasswordHash(int userId) = 0;

    /**
     * @brief 更新用户密码哈希。
     * @param userId 用户 ID
     * @param newPasswordHash 新密码哈希
     */
    virtual void changePassword(int userId,
                                const std::string& newPasswordHash) = 0;

    /**
     * @brief 注销账户（删除用户及关联数据）。
     * @param userId 用户 ID
     */
    virtual void deleteAccount(int userId) = 0;

    /**
     * @brief 上传头像，返回头像 URL 与资源 ID。
     * @param userId 用户 ID
     * @param filePath 待上传的本地图片路径
     * @return 头像响应（avatar_id 与 avatar_url）
     */
    virtual models::AvatarUploadResponse uploadAvatar(
        int userId, const std::string& filePath) = 0;

    /**
     * @brief 查询用户饮食偏好。
     * @param userId 用户 ID
     * @return 偏好数据（喜爱/禁忌/健康目标）
     */
    virtual models::UserPreferences getPreferences(int userId) = 0;

    /**
     * @brief 更新用户饮食偏好。
     * @param userId 用户 ID
     * @param prefs 新的偏好数据
     */
    virtual void updatePreferences(int userId,
                                   const models::UserPreferences& prefs) = 0;

    /**
     * @brief 查询用户健康条件列表。
     * @param userId 用户 ID
     * @return 健康条件（如慢性病/过敏原）
     */
    virtual std::vector<std::string> getHealthConditions(int userId) = 0;

    /**
     * @brief 录入/更新健康指标。
     * @param userId 用户 ID
     * @param req 身高体重与健康条件
     * @return 更新后的健康指标（含忌口建议）
     */
    virtual models::HealthProfileResponse updateHealthProfile(
        int userId, const models::HealthProfileRequest& req) = 0;

    /**
     * @brief 查询健康指标。
     * @param userId 用户 ID
     * @return 健康指标（含忌口建议）
     */
    virtual models::HealthProfileResponse getHealthProfile(int userId) = 0;

    /**
     * @brief 查询收藏列表（分页）。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param group 分组名筛选，空串表示所有分组
     * @return 分页的收藏列表
     */
    virtual models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group) = 0;

    /**
     * @brief 查询收藏分组列表。
     * @param userId 用户 ID
     * @return 分组列表（含各自收藏数量）
     */
    virtual std::vector<models::FavoriteGroup> getFavoriteGroups(int userId) = 0;

    /**
     * @brief 创建收藏分组。
     * @param userId 用户 ID
     * @param req 分组名
     * @return 创建的分组对象
     */
    virtual models::FavoriteGroup createFavoriteGroup(
        int userId, const models::CreateGroupRequest& req) = 0;

    /**
     * @brief 更新收藏分组。
     * @param userId 用户 ID
     * @param groupId 分组 ID
     * @param req 新分组名
     */
    virtual void updateFavoriteGroup(int userId, int groupId,
                                     const models::UpdateGroupRequest& req) = 0;

    /**
     * @brief 删除收藏分组。
     * @param userId 用户 ID
     * @param groupId 分组 ID
     */
    virtual void deleteFavoriteGroup(int userId, int groupId) = 0;

    /**
     * @brief 更新收藏项属性（分组/可见性）。
     * @param userId 用户 ID
     * @param favoriteId 收藏记录 ID
     * @param req 待更新的属性
     */
    virtual void updateFavoriteItem(int userId, int favoriteId,
                                    const models::UpdateFavoriteRequest& req) = 0;

    /**
     * @brief 批量删除收藏。
     * @param userId 用户 ID
     * @param req 待删除的收藏 ID 列表
     */
    virtual void batchDeleteFavorites(
        int userId, const models::BatchDeleteFavoritesRequest& req) = 0;

    /**
     * @brief 查询通知列表（分页）。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param type 类型筛选："system"/"review"/"interaction"，空串表示全部
     * @return 分页的通知列表
     */
    virtual models::PagedNotifications getNotifications(int userId, int page,
                                                        int size,
                                                        const std::string& type) = 0;

    /**
     * @brief 标记单条通知已读。
     * @param userId 用户 ID
     * @param notificationId 通知 ID
     */
    virtual void markNotificationRead(int userId, int notificationId) = 0;

    /**
     * @brief 全部通知标记已读。
     * @param userId 用户 ID
     */
    virtual void markAllNotificationsRead(int userId) = 0;

    /**
     * @brief 删除通知。
     * @param userId 用户 ID
     * @param notificationId 通知 ID
     */
    virtual void deleteNotification(int userId, int notificationId) = 0;
};

} // namespace gocook::repository
