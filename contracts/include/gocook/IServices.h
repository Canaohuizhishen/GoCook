// IServices.h （仅修改 IInventoryService::upsertInventory 返回类型）
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

        /// 获取我的投稿列表（需认证）
        virtual models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                             int page, int size) = 0;

        /// 编辑未审核的菜谱（需认证）
        virtual void editRecipe(int userId, int recipeId,
                                const models::EditRecipeRequest& updates) = 0;

        /// 切换收藏状态（需认证）
        virtual void toggleFavorite(int userId, int recipeId) = 0;

        /// 评分与评论（需认证）
        virtual void rateRecipe(int userId, int recipeId,
                                const models::RateRecipeRequest& request) = 0;
    };

    // ======================== 用户服务接口 ========================
    class IUserService {
    public:
        virtual ~IUserService() = default;

        /// 用户注册（无需认证）
        virtual void registerUser(const models::RegisterRequest& request) = 0;

        /// 用户登录，返回 token 等信息（无需认证）
        virtual models::LoginResponse login(const models::LoginRequest& request) = 0;

        /// 获取当前用户信息（需认证）
        virtual models::UserProfile getCurrentUser(int userId) = 0;

        /// 更新当前用户个人资料（需认证）
        virtual models::UserProfile updateProfile(int userId,
                                                  const models::UpdateProfileRequest& profile) = 0;

        /// 获取用户饮食偏好（需认证）
        virtual models::UserPreferences getPreferences(int userId) = 0;

        /// 更新用户饮食偏好（需认证）
        virtual void updatePreferences(int userId,
                                       const models::UserPreferences& prefs) = 0;

        /// 录入/更新健康指标（需认证）
        virtual models::HealthProfileResponse updateHealthProfile(
            int userId, const models::HealthProfileRequest& healthProfile) = 0;

        /// 获取用户收藏列表（分页，需认证）
        virtual models::PagedFavorites getFavorites(int userId, int page, int size) = 0;
    };

    // ======================== 库存服务接口 ========================
    class IInventoryService {
    public:
        virtual ~IInventoryService() = default;

        /// 获取当前用户库存（分页，需认证）
        virtual models::PagedInventory getInventory(int userId, int page, int size) = 0;

        /// 添加/更新库存项（需认证），返回新增或更新后的库存项ID
        virtual int upsertInventory(int userId,
                                    const models::UpsertInventoryRequest& item) = 0;

        /// 删除库存项（需认证）
        virtual void deleteInventoryItem(int userId, int itemId) = 0;

        /// 获取/生成购物清单（需认证）
        virtual models::ShoppingList getShoppingList(int userId,
                                                     std::optional<int> planId = std::nullopt) = 0;

        /// 更新购物清单项状态（需认证）
        virtual void updateShoppingListItem(int userId, int itemId,
                                            const models::UpdateShoppingItemRequest& request) = 0;

        /// 批量添加购物清单项（需认证）
        virtual models::BatchShoppingResponse batchAddShoppingItems(
            int userId, const std::vector<models::BatchShoppingItem>& items) = 0;
    };

    // ======================== 膳食计划服务接口 ========================
    class IMealPlanService {
    public:
        virtual ~IMealPlanService() = default;

        /// 创建膳食计划项（需认证）
        virtual int createMealPlan(int userId,
                                   const models::MealPlanRequest& planData) = 0;

        /// 获取膳食计划列表（需认证）
        virtual models::MealPlansResponse getMealPlans(int userId,
                                                       const std::string& startDate,
                                                       const std::string& endDate,
                                                       int page, int size) = 0;

        /// 获取膳食计划日历视图详情（需认证）
        virtual models::MealPlanCalendar getMealPlanDetail(int userId,
                                                           const std::string& startDate,
                                                           const std::string& endDate) = 0;

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
    };

} // namespace gocook::services