#pragma once

#include <gocook/IUserRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 用户仓库的 PostgreSQL 实现，承载 IUserRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 UserServiceImpl 调用。
 */
class PgUserRepository : public gocook::repository::IUserRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgUserRepository(ConnectionPool& db) : db_(db) {}

    // 按用户名查询用户认证信息（含密码哈希，用于登录验证）
    std::optional<gocook::repository::UserAuthInfo> findByUsername(const std::string& username) override;
    // 检查邮箱是否已被注册
    bool existsByEmail(const std::string& email) override;
    // 创建新用户
    void createUser(const std::string& username,
                    const std::string& passwordHash,
                    const std::string& email) override;

    // 按 ID 查询用户公开资料
    std::optional<gocook::models::UserProfile> findById(int userId) override;
    // 通过邮箱查找用户 ID（用于忘记密码）
    std::optional<int> findIdByEmail(const std::string& email) override;
    // 通过用户名+邮箱联合查找用户 ID（忘记密码时双重验证身份）
    std::optional<int> findIdByUsernameAndEmail(const std::string& username,
                                                 const std::string& email) override;
    // 创建密码重置令牌（SQL 层统一设置 15 分钟过期）
    void createPasswordResetToken(int userId, const std::string& token) override;
    // 按令牌查找对应用户 ID（仅返回未使用且未过期的令牌）
    std::optional<int> findUserIdByResetToken(const std::string& token) override;
    // 标记密码重置令牌为已使用
    void markResetTokenUsed(const std::string& token) override;
    // 在同一事务中重置密码并标记令牌已用（防止令牌重放）
    void resetPasswordAndMarkTokenUsed(int userId,
                                       const std::string& newPasswordHash,
                                       const std::string& token) override;
    // 更新用户个人资料
    void updateProfile(int userId,
                       const gocook::models::UpdateProfileRequest& profile) override;
    // 获取用户密码哈希（修改密码时验证原密码用）
    std::string getPasswordHash(int userId) override;
    // 更新密码哈希
    void changePassword(int userId, const std::string& newPasswordHash) override;
    // 注销账户
    void deleteAccount(int userId) override;

    // 保存头像文件并记录头像信息，返回 URL 与 ID
    gocook::models::AvatarUploadResponse uploadAvatar(
        int userId, const std::string& filePath) override;

    // 查询用户饮食偏好
    gocook::models::UserPreferences getPreferences(int userId) override;
    // 更新用户饮食偏好
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override;
    // 查询用户健康条件列表
    std::vector<std::string> getHealthConditions(int userId) override;
    // 录入/更新健康指标
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& req) override;
    // 查询健康指标（含忌口建议）
    gocook::models::HealthProfileResponse getHealthProfile(int userId) override;

    // 查询收藏列表（分页，可按分组筛选）
    gocook::models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group) override;
    // 查询收藏分组列表
    std::vector<gocook::models::FavoriteGroup> getFavoriteGroups(int userId) override;
    // 创建收藏分组，返回创建的分组对象
    gocook::models::FavoriteGroup createFavoriteGroup(
        int userId, const gocook::models::CreateGroupRequest& req) override;
    // 更新收藏分组
    void updateFavoriteGroup(int userId, int groupId,
                             const gocook::models::UpdateGroupRequest& req) override;
    // 删除收藏分组
    void deleteFavoriteGroup(int userId, int groupId) override;
    // 更新收藏项属性（分组/可见性）
    void updateFavoriteItem(int userId, int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& req) override;
    // 批量删除收藏
    void batchDeleteFavorites(
        int userId, const gocook::models::BatchDeleteFavoritesRequest& req) override;

    // 查询通知列表（分页，可按类型筛选）
    gocook::models::PagedNotifications getNotifications(int userId, int page,
                                                        int size,
                                                        const std::string& type) override;
    // 标记单条通知已读
    void markNotificationRead(int userId, int notificationId) override;
    // 全部通知标记已读
    void markAllNotificationsRead(int userId) override;
    // 删除通知
    void deleteNotification(int userId, int notificationId) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
