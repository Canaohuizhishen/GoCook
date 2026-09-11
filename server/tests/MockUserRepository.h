#pragma once

#include <gocook/IUserRepository.h>
#include <gmock/gmock.h>

/**
 * @brief IUserRepository 的 Google Mock 替身，用于服务层单元测试。
 *
 * 在测试中通过 EXPECT_CALL 为每个方法设置预期调用与返回值，
 * 方法契约见 IUserRepository.h 接口文档。
 */
class MockUserRepository : public gocook::repository::IUserRepository {
public:
    // 按用户名查询用户认证信息（登录验证用）
    MOCK_METHOD(std::optional<gocook::repository::UserAuthInfo>, findByUsername,
                (const std::string&), (override));
    // 检查邮箱是否已被注册
    MOCK_METHOD(bool, existsByEmail, (const std::string&), (override));
    // 创建新用户
    MOCK_METHOD(void, createUser,
                (const std::string&, const std::string&, const std::string&), (override));
    // 创建/覆盖待验证注册记录（两段式注册第一步）
    MOCK_METHOD(void, upsertPendingRegistration,
                (const std::string&, const std::string&, const std::string&, const std::string&), (override));
    // 校验验证码并原子创建用户（两段式注册第二步）
    MOCK_METHOD(gocook::models::RegistrationOutcome, createUserFromPendingRegistration,
                (const std::string&, const std::string&), (override));
    // 按 ID 查询用户公开资料
    MOCK_METHOD(std::optional<gocook::models::UserProfile>, findById, (int), (override));
    // 通过邮箱查找用户 ID
    MOCK_METHOD(std::optional<int>, findIdByEmail, (const std::string&), (override));
    // 通过用户名+邮箱联合查找用户 ID
    MOCK_METHOD(std::optional<int>, findIdByUsernameAndEmail,
                (const std::string&, const std::string&), (override));
    // 创建密码重置令牌
    MOCK_METHOD(void, createPasswordResetToken,
                (int, const std::string&), (override));
    // 按令牌查找用户 ID
    MOCK_METHOD(std::optional<int>, findUserIdByResetToken, (const std::string&), (override));
    // 标记密码重置令牌为已使用
    MOCK_METHOD(void, markResetTokenUsed, (const std::string&), (override));
    // 重置密码并标记令牌已用
    MOCK_METHOD(void, resetPasswordAndMarkTokenUsed,
                (int, const std::string&, const std::string&), (override));
    // 更新用户个人资料
    MOCK_METHOD(void, updateProfile,
                (int, const gocook::models::UpdateProfileRequest&), (override));
    // 修改密码
    MOCK_METHOD(void, changePassword, (int, const std::string&), (override));
    // 注销账户
    MOCK_METHOD(void, deleteAccount, (int), (override));
    // 获取用户密码哈希
    MOCK_METHOD(std::string, getPasswordHash, (int), (override));
    // 上传头像
    MOCK_METHOD(gocook::models::AvatarUploadResponse, uploadAvatar,
                (int, const std::string&), (override));
    // 获取用户饮食偏好
    MOCK_METHOD(gocook::models::UserPreferences, getPreferences, (int), (override));
    // 更新用户饮食偏好
    MOCK_METHOD(void, updatePreferences,
                (int, const gocook::models::UserPreferences&), (override));
    // 获取用户健康条件列表
    MOCK_METHOD(std::vector<std::string>, getHealthConditions, (int), (override));
    // 录入/更新健康指标
    MOCK_METHOD(gocook::models::HealthProfileResponse, updateHealthProfile,
                (int, const gocook::models::HealthProfileRequest&), (override));
    // 获取健康指标
    MOCK_METHOD(gocook::models::HealthProfileResponse, getHealthProfile,
                (int), (override));
    // 查询收藏列表
    MOCK_METHOD(gocook::models::PagedFavorites, getFavorites,
                (int, int, int, const std::string&), (override));
    // 查询收藏分组列表
    MOCK_METHOD(std::vector<gocook::models::FavoriteGroup>, getFavoriteGroups,
                (int), (override));
    // 创建收藏分组
    MOCK_METHOD(gocook::models::FavoriteGroup, createFavoriteGroup,
                (int, const gocook::models::CreateGroupRequest&), (override));
    // 更新收藏分组
    MOCK_METHOD(void, updateFavoriteGroup,
                (int, int, const gocook::models::UpdateGroupRequest&), (override));
    // 删除收藏分组
    MOCK_METHOD(void, deleteFavoriteGroup, (int, int), (override));
    // 更新收藏项属性
    MOCK_METHOD(void, updateFavoriteItem,
                (int, int, const gocook::models::UpdateFavoriteRequest&), (override));
    // 批量删除收藏
    MOCK_METHOD(void, batchDeleteFavorites,
                (int, const gocook::models::BatchDeleteFavoritesRequest&), (override));
    // 查询通知列表
    MOCK_METHOD(gocook::models::PagedNotifications, getNotifications,
                (int, int, int, const std::string&), (override));
    // 标记单条通知已读
    MOCK_METHOD(void, markNotificationRead, (int, int), (override));
    // 全部通知标记已读
    MOCK_METHOD(void, markAllNotificationsRead, (int), (override));
    // 删除通知
    MOCK_METHOD(void, deleteNotification, (int, int), (override));
};
