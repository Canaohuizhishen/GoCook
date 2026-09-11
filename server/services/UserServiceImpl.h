#pragma once

#include <gocook/IServices.h>
#include <gocook/IUserRepository.h>
#include <string>
#include <memory>

/**
 * @brief 用户服务实现，承载 IUserService 接口定义的全部用户业务逻辑。
 *
 * 依赖用户仓库抽象与 JWT 签名密钥，负责注册/登录/资料/收藏/通知等业务，
 * 由 UserHandler 调用，不直接接触数据库。
 */
class UserServiceImpl : public gocook::services::IUserService {
public:
    /**
     * @brief 构造函数，注入用户仓库与 JWT 签名密钥
     * @param userRepo  用户仓库抽象指针
     * @param jwtSecret JWT HS256 签名密钥，需与 AuthMiddleware 使用相同密钥
     */
    explicit UserServiceImpl(std::unique_ptr<gocook::repository::IUserRepository> userRepo,
                             const std::string& jwtSecret)
        : userRepo_(std::move(userRepo)), jwt_secret_(jwtSecret) {}

    // 用户注册（两段式第一步：写入待验证记录并发送验证码邮件）
    void registerUser(const gocook::models::RegisterRequest& request) override;
    // 完成注册（两段式第二步：验证码核验后建号）
    void verifyRegistration(const std::string& email, const std::string& token) override;
    // 用户登录，验证密码并签发 JWT Token
    gocook::models::LoginResponse login(const gocook::models::LoginRequest& request) override;

    // 请求密码重置：发送重置邮件；SMTP 未配置时返回重置令牌（开发模式）
    std::optional<std::string> requestPasswordReset(const std::string& username,
                                                     const std::string& email) override;
    // 使用重置令牌设置新密码
    void resetPassword(const std::string& token,
                       const std::string& newPassword) override;
    // 获取当前用户资料
    gocook::models::UserProfile getCurrentUser(int userId) override;
    // 更新当前用户个人资料，返回更新后的资料
    gocook::models::UserProfile updateProfile(int userId,
                                               const gocook::models::UpdateProfileRequest& profile) override;
    // 修改密码（需验证原密码）
    void changePassword(int userId,
                        const std::string& currentPassword,
                        const std::string& newPassword) override;
    // 注销账户
    void deleteAccount(int userId) override;
    // 上传头像，返回头像 URL 与资源 ID
    gocook::models::AvatarUploadResponse uploadAvatar(int userId,
                                                       const std::string& filePath) override;
    // 获取用户饮食偏好
    gocook::models::UserPreferences getPreferences(int userId) override;
    // 更新用户饮食偏好
    void updatePreferences(int userId,
                           const gocook::models::UserPreferences& prefs) override;
    // 录入/更新健康指标
    gocook::models::HealthProfileResponse updateHealthProfile(
        int userId, const gocook::models::HealthProfileRequest& healthProfile) override;
    // 获取健康指标（含忌口建议）
    gocook::models::HealthProfileResponse getHealthProfile(int userId) override;
    // 获取收藏列表（分页，可按分组筛选）
    gocook::models::PagedFavorites getFavorites(int userId, int page, int size,
                                                const std::string& group = "") override;
    // 获取收藏分组列表
    std::vector<gocook::models::FavoriteGroup> getFavoriteGroups(int userId) override;
    // 创建收藏分组，返回创建的分组对象
    gocook::models::FavoriteGroup createFavoriteGroup(int userId,
                                                       const gocook::models::CreateGroupRequest& request) override;
    // 更新收藏分组
    void updateFavoriteGroup(int userId, int groupId,
                             const gocook::models::UpdateGroupRequest& request) override;
    // 删除收藏分组
    void deleteFavoriteGroup(int userId, int groupId) override;
    // 更新收藏项属性（移动分组/可见性）
    void updateFavoriteItem(int userId, int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& request) override;
    // 批量删除收藏
    void batchDeleteFavorites(int userId,
                              const gocook::models::BatchDeleteFavoritesRequest& request) override;
    // 获取通知列表（分页，可按类型筛选）
    gocook::models::PagedNotifications getNotifications(int userId, int page, int size,
                                                        const std::string& type = "") override;
    // 标记单条通知已读
    void markNotificationRead(int userId, int notificationId) override;
    // 全部通知标记已读
    void markAllNotificationsRead(int userId) override;
    // 删除通知
    void deleteNotification(int userId, int notificationId) override;

private:
    std::unique_ptr<gocook::repository::IUserRepository> userRepo_; ///< 用户仓库抽象

    // 生成 JWT Token（HS256，含 userId/username/role）
    std::string generateToken(int userId, const std::string& username, const std::string& role);
    // 计算密码哈希
    std::string hashPassword(const std::string& plain);
    // 校验明文密码与哈希是否匹配
    bool validatePassword(const std::string& plain, const std::string& hash);

    std::string jwt_secret_; ///< JWT HS256 签名密钥
};
