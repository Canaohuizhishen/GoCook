#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>
#include <string>

namespace gocook::services {

    // 自定义业务异常，由 Handler 转换为 HTTP 状态码
    class ServiceException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    // ======================== 菜谱服务接口 ========================
    class IRecipeService {
    public:
        virtual ~IRecipeService() = default;

        /// 获取公开菜谱列表（无需认证）
        virtual models::PagedRecipes getPublicRecipes(int page, int size,
                                                      const nlohmann::json& filters) = 0;

        /// 关键词搜索菜谱
        virtual models::PagedRecipes searchRecipes(const std::string& keyword,
                                                   int page, int size,
                                                   const nlohmann::json& filters) = 0;

        /// 智能推荐菜谱（需用户偏好和库存）
        virtual models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                      int page, int size) = 0;

        /// 获取菜谱详情
        virtual models::RecipeDetail getRecipeDetail(int recipeId) = 0;

        /// 获取菜谱关联视频列表
        virtual std::vector<models::RecipeVideo> getRecipeVideos(int recipeId) = 0;

        /// 获取菜谱评分与评论（分页）
        virtual models::PagedRatings getRecipeRatings(int recipeId, int page, int size) = 0;

        /// 投稿新菜谱（需认证）
        virtual models::SubmitRecipeResponse submitRecipe(int userId,
                                                          const models::SubmitRecipeRequest& data) = 0;

        /// 获取我的投稿列表（需认证，可选按状态筛选）
        /// @param status 可选筛选状态："pending", "approved", "rejected"，空字符串表示全部
        virtual models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                             int page, int size,
                                                             const std::string& status = "") = 0;

        /// 编辑未审核的菜谱（需认证）
        virtual void editRecipe(int userId, int recipeId,
                                const models::EditRecipeRequest& updates) = 0;

        /// 切换收藏状态（需认证）
        /// @param groupId 可选分组ID，不传则使用默认分组
        /// @param isPublic 可选可见性，nullopt 表示保持默认或当前状态
        virtual void toggleFavorite(int userId, int recipeId,
                                    std::optional<int> groupId = std::nullopt,
                                    std::optional<bool> isPublic = std::nullopt) = 0;

        /// 评分与评论（需认证）
        virtual void rateRecipe(int userId, int recipeId,
                                const models::RateRecipeRequest& request) = 0;

        /// 修改评论（需认证）
        virtual void updateRating(int userId, int recipeId, int ratingId,
                                  const models::RateRecipeRequest& request) = 0;

        /// 删除评论（需认证）
        virtual void deleteRating(int userId, int recipeId, int ratingId) = 0;

        /// 获取当前用户的所有评论列表
        virtual models::PagedUserRatings getMyRatings(int userId, int page, int size) = 0;

        /// 获取独立营养报告（对应 API 4.15）
        virtual models::NutritionReport getRecipeNutrition(int recipeId) = 0;
    };

    // ======================== 用户服务接口 ========================
    class IUserService {
    public:
        virtual ~IUserService() = default;

        /// 用户注册（无需认证）
        virtual void registerUser(const models::RegisterRequest& request) = 0;

        /// 用户登录，返回 token 等信息（无需认证）
        virtual models::LoginResponse login(const models::LoginRequest& request) = 0;

        /// 发送密码重置邮件（无需认证，对应 API 3.11.2）
        virtual void requestPasswordReset(const std::string& email) = 0;

        /// 重置密码（无需认证）
        virtual void resetPassword(const std::string& token,
                                   const std::string& newPassword) = 0;

        /// 获取当前用户信息（需认证）
        virtual models::UserProfile getCurrentUser(int userId) = 0;

        /// 更新当前用户个人资料（需认证），返回更新后的资料
        virtual models::UserProfile updateProfile(int userId,
                                                  const models::UpdateProfileRequest& profile) = 0;

        /// 修改密码（需认证）
        virtual void changePassword(int userId,
                                    const std::string& currentPassword,
                                    const std::string& newPassword) = 0;

        /// 注销账户（需认证）
        virtual void deleteAccount(int userId) = 0;

        /// 上传头像，返回 URL 和 ID（需认证）
        virtual models::AvatarUploadResponse uploadAvatar(int userId,
                                                          const std::string& filePath) = 0;

        /// 获取用户饮食偏好（需认证）
        virtual models::UserPreferences getPreferences(int userId) = 0;

        /// 更新用户饮食偏好（需认证）
        virtual void updatePreferences(int userId,
                                       const models::UserPreferences& prefs) = 0;

        /// 录入/更新健康指标（需认证）
        virtual models::HealthProfileResponse updateHealthProfile(
            int userId, const models::HealthProfileRequest& healthProfile) = 0;

        /// 获取用户收藏列表（分页，需认证，可选按分组筛选）
        /// @param group 可选分组名，空字符串表示所有分组
        virtual models::PagedFavorites getFavorites(int userId, int page, int size,
                                                    const std::string& group = "") = 0;

        /// 获取收藏分组列表
        virtual std::vector<models::FavoriteGroup> getFavoriteGroups(int userId) = 0;

        /// 创建收藏分组，返回创建的分组对象
        virtual models::FavoriteGroup createFavoriteGroup(int userId,
                                                          const models::CreateGroupRequest& request) = 0;

        /// 更新收藏分组
        virtual void updateFavoriteGroup(int userId, int groupId,
                                         const models::UpdateGroupRequest& request) = 0;

        /// 删除收藏分组
        virtual void deleteFavoriteGroup(int userId, int groupId) = 0;

        /// 更新收藏项属性
        virtual void updateFavoriteItem(int userId, int favoriteId,
                                        const models::UpdateFavoriteRequest& request) = 0;

        /// 批量删除收藏
        virtual void batchDeleteFavorites(int userId,
                                          const models::BatchDeleteFavoritesRequest& request) = 0;

        /// 获取通知列表（分页，可选按类型筛选）
        /// @param type 可选通知类型："system", "review", "interaction"，空字符串表示所有
        virtual models::PagedNotifications getNotifications(int userId, int page, int size,
                                                            const std::string& type = "") = 0;

        /// 标记通知已读
        virtual void markNotificationRead(int userId, int notificationId) = 0;

        /// 全部标记已读
        virtual void markAllNotificationsRead(int userId) = 0;

        /// 删除通知
        virtual void deleteNotification(int userId, int notificationId) = 0;
    };

    // ======================== 库存与购物清单服务接口 ========================
    class IInventoryService {
    public:
        virtual ~IInventoryService() = default;

        /// 获取当前用户库存（分页，需认证）
        virtual models::PagedInventory getInventory(int userId, int page, int size) = 0;

        /// 添加/更新库存项（需认证），返回库存项 ID
        virtual int upsertInventory(int userId,
                                    const models::UpsertInventoryRequest& item) = 0;

        /// 删除库存项（需认证）
        virtual void deleteInventoryItem(int userId, int itemId) = 0;

        // ---------- 购物清单管理（多清单模型） ----------

        /// 获取用户的购物清单列表
        virtual std::vector<models::ShoppingListSummary> getShoppingLists(int userId) = 0;

        /// 创建购物清单，返回新清单 ID
        /// 若请求中提供 plan_id，则基于该膳食计划自动生成清单内容
        virtual int createShoppingList(int userId,
                                       const models::CreateShoppingListRequest& request) = 0;

        /// 获取指定购物清单详情
        virtual models::ShoppingList getShoppingListDetail(int userId, int listId) = 0;

        /// 删除购物清单
        virtual void deleteShoppingList(int userId, int listId) = 0;

        /// 更新购物清单项状态（需传入 listId 和 itemId）
        virtual void updateShoppingListItem(int userId, int listId, int itemId,
                                            const models::UpdateShoppingItemRequest& request) = 0;

        /// 批量添加购物清单项（需传入 listId）
        virtual models::BatchShoppingResponse batchAddShoppingItems(
            int userId, int listId, const std::vector<models::BatchShoppingItem>& items) = 0;

        /// 导出购物清单，返回内容（文本为纯文本，图片为 base64 编码）
        /// @param format "text" 或 "image"
        virtual std::string exportShoppingList(int userId, int listId,
                                               const std::string& format) = 0;
    };

    // ======================== 膳食计划服务接口 ========================
    class IMealPlanService {
    public:
        virtual ~IMealPlanService() = default;

        /// 创建膳食计划项（需认证），返回计划项 ID
        virtual int createMealPlan(int userId,
                                   const models::MealPlanRequest& planData) = 0;

        /// 获取膳食计划列表（需认证）
        virtual models::MealPlansResponse getMealPlans(int userId,
                                                       const std::string& startDate,
                                                       const std::string& endDate,
                                                       int page, int size) = 0;

        /// 获取膳食计划日历视图详情（需认证），支持分页
        /// @param page 页码，每页返回的天数
        /// @param size 每页天数，最大 30
        virtual models::PagedCalendarDays getMealPlanDetail(int userId,
                                                            const std::string& startDate,
                                                            const std::string& endDate,
                                                            int page, int size) = 0;

        /// 更新膳食计划项（需认证）
        virtual void updateMealPlan(int userId, int planId,
                                    const models::MealPlanRequest& updates) = 0;

        /// 删除膳食计划项（需认证）
        virtual void deleteMealPlan(int userId, int planId) = 0;

        /// 获取营养摄入趋势（需认证）
        virtual models::NutritionTrendResponse getNutritionTrend(int userId,
                                                                 const std::string& startDate,
                                                                 const std::string& endDate) = 0;
    };

    // ======================== 公告服务接口（公开） ========================
    class IAnnouncementService {
    public:
        virtual ~IAnnouncementService() = default;

        /// 获取系统公告列表（分页，无需认证）
        virtual models::PagedAnnouncements getAnnouncements(int page, int size) = 0;
    };

    // ======================== 管理员服务接口 ========================
    class IAdminService {
    public:
        virtual ~IAdminService() = default;

        /// 获取用户列表（管理员）
        virtual models::PagedUsers getUsers(int page, int size,
                                            const nlohmann::json& filters) = 0;

        /// 创建用户账户（管理员）
        virtual void createUser(const models::CreateUserRequest& userData) = 0;

        /// 修改用户信息（管理员）
        virtual void updateUser(int userId,
                                const models::UpdateUserRequest& updates) = 0;

        /// 冻结/解封用户（管理员）
        virtual void setUserStatus(int userId,
                                   const models::SetUserStatusRequest& request) = 0;

        /// 删除用户（管理员）
        virtual void deleteUser(int userId) = 0;

        /// 获取待审核菜谱列表（管理员）
        virtual models::PagedPendingRecipes getPendingRecipes(int page, int size) = 0;

        /// 审核通过菜谱（管理员）
        virtual void approveRecipe(int recipeId) = 0;

        /// 审核拒绝菜谱（管理员）
        virtual void rejectRecipe(int recipeId,
                                  const models::RejectRecipeRequest& request) = 0;

        /// 批量审核菜谱（管理员）
        virtual models::BatchReviewResponse batchReviewRecipes(
            const models::BatchReviewRequest& request) = 0;

        /// 发布系统公告（管理员）
        virtual void publishAnnouncement(const models::AnnouncementRequest& request) = 0;

        /// 发送系统通知（管理员）
        virtual models::NotificationResponse sendNotification(
            const models::NotificationRequest& notification) = 0;

        /// 获取运营数据统计
        virtual models::StatisticsData getStatistics() = 0;

        /// 查询管理员操作日志
        /// @param type 可选日志类型筛选
        /// @param userId 可选操作者ID筛选
        virtual models::PagedAdminLogs getAdminLogs(int page, int size,
                                                    const std::string& type = "",
                                                    int userId = 0) = 0;

        /// 查询全局用户行为日志
        /// @param userId 可选用户ID筛选
        /// @param action 可选行为类型筛选
        virtual models::PagedActivityLogs getActivityLogs(int page, int size,
                                                          int userId = 0,
                                                          const std::string& action = "") = 0;
    };

} // namespace gocook::services