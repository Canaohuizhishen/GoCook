#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>  // 仅用于 filters 参数
#include <gocook/DataModels.h>

// ======================== 回调类型定义 ========================
// 简单成功/失败回调
using SuccessCallback = std::function<void(bool success, const std::string& error)>;

// 登录专用回调（现在使用 LoginResponse 结构体）
using LoginCallback = std::function<void(bool success,
                                         const gocook::models::LoginResponse& data,
                                         const std::string& error)>;

// 带整数 id 的回调（通常用于创建资源）
using IntCallback = std::function<void(bool success, int id, const std::string& error)>;

// 菜谱投稿专用回调（返回 id 和 status）
using SubmitRecipeCallback = std::function<void(bool success,
                                                const gocook::models::SubmitRecipeResponse& data,
                                                const std::string& error)>;

// 用户相关
using UserProfileCallback = std::function<void(bool success,
                                               const gocook::models::UserProfile& data,
                                               const std::string& error)>;

using PreferencesCallback = std::function<void(bool success,
                                               const gocook::models::UserPreferences& data,
                                               const std::string& error)>;

using HealthProfileCallback = std::function<void(bool success,
                                                 const gocook::models::HealthProfileResponse& data,
                                                 const std::string& error)>;

// 分页收藏
using FavoritesCallback = std::function<void(bool success,
                                             const gocook::models::PagedFavorites& data,
                                             const std::string& error)>;

// 菜谱相关
using PagedRecipesCallback = std::function<void(bool success,
                                                const gocook::models::PagedRecipes& data,
                                                const std::string& error)>;

using PagedRecommendedRecipesCallback = std::function<void(bool success,
                                                           const gocook::models::PagedRecommendedRecipes& data,
                                                           const std::string& error)>;

using RecipeDetailCallback = std::function<void(bool success,
                                                const gocook::models::RecipeDetail& data,
                                                const std::string& error)>;

using RecipeVideosCallback = std::function<void(bool success,
                                                const std::vector<gocook::models::RecipeVideo>& data,
                                                const std::string& error)>;

using PagedRatingsCallback = std::function<void(bool success,
                                                const gocook::models::PagedRatings& data,
                                                const std::string& error)>;

using PagedMyRecipesCallback = std::function<void(bool success,
                                                  const gocook::models::PagedMyRecipes& data,
                                                  const std::string& error)>;

// 库存管理
using PagedInventoryCallback = std::function<void(bool success,
                                                  const gocook::models::PagedInventory& data,
                                                  const std::string& error)>;

// 购物清单
using ShoppingListCallback = std::function<void(bool success,
                                                const gocook::models::ShoppingList& data,
                                                const std::string& error)>;

using BatchShoppingCallback = std::function<void(bool success,
                                                 const gocook::models::BatchShoppingResponse& data,
                                                 const std::string& error)>;

// 膳食计划
using MealPlanCalendarCallback = std::function<void(bool success,
                                                    const gocook::models::MealPlanCalendar& data,
                                                    const std::string& error)>;

using NutritionTrendCallback = std::function<void(bool success,
                                                  const gocook::models::NutritionTrendResponse& data,
                                                  const std::string& error)>;

using PagedMealPlansCallback = std::function<void(bool success,
                                                  const gocook::models::MealPlansResponse& data,
                                                  const std::string& error)>;

// 公告
using PagedAnnouncementsCallback = std::function<void(bool success,
                                                      const gocook::models::PagedAnnouncements& data,
                                                      const std::string& error)>;

// 管理员
using PagedUsersCallback = std::function<void(bool success,
                                              const gocook::models::PagedUsers& data,
                                              const std::string& error)>;

using PagedPendingRecipesCallback = std::function<void(bool success,
                                                       const gocook::models::PagedPendingRecipes& data,
                                                       const std::string& error)>;

using BatchReviewCallback = std::function<void(bool success,
                                               const gocook::models::BatchReviewResponse& data,
                                               const std::string& error)>;

using NotificationCallback = std::function<void(bool success,
                                                const gocook::models::NotificationResponse& data,
                                                const std::string& error)>;

/**
 * @brief 服务端 API 抽象接口，定义所有与后端交互的方法。
 *
 * 具体实现类（HttpGoCookApi）负责通过 HTTP 发送请求。
 * 客户端高层模块只依赖本接口，符合依赖倒置原则。
 * 所有业务数据均使用强类型结构体（定义于 DataModels.h），
 * 仅筛选条件 filters 因结构多变暂时保留为 nlohmann::json。
 */
class IGoCookApi {
public:
    virtual ~IGoCookApi() = default;

    // ---------- 认证（公开接口） ----------
    /**
     * @brief 用户注册
     * @param request 注册请求，包含用户名和密码
     * @param callback 回调 (success, error)
     */
    virtual void registerUser(const gocook::models::RegisterRequest& request,
                              SuccessCallback callback) = 0;

    /**
     * @brief 用户登录
     * @param request 登录请求，包含用户名和密码
     * @param callback 回调 (success, LoginResponse, error)
     */
    virtual void login(const gocook::models::LoginRequest& request,
                       LoginCallback callback) = 0;

    // ---------- 用户相关（需认证） ----------
    /**
     * @brief 获取当前登录用户信息
     * @param callback 回调 (success, profile, error)
     */
    virtual void getCurrentUser(UserProfileCallback callback) = 0;

    /**
     * @brief 更新当前用户个人资料
     * @param profile 用户资料，可部分更新
     * @param callback 回调 (success, error)
     */
    virtual void updateProfile(const gocook::models::UpdateProfileRequest& profile,
                               SuccessCallback callback) = 0;

    /**
     * @brief 获取当前用户的饮食偏好
     * @param callback 回调 (success, prefs, error)
     */
    virtual void getPreferences(PreferencesCallback callback) = 0;

    /**
     * @brief 更新当前用户的饮食偏好
     * @param prefs 偏好数据
     * @param callback 回调 (success, error)
     */
    virtual void updatePreferences(const gocook::models::UserPreferences& prefs,
                                   SuccessCallback callback) = 0;

    /**
     * @brief 录入/更新健康指标
     * @param healthProfile 健康指标
     * @param callback 回调 (success, avoidances, error)
     */
    virtual void updateHealthProfile(const gocook::models::HealthProfileRequest& healthProfile,
                                     HealthProfileCallback callback) = 0;

    /**
     * @brief 获取当前用户的收藏列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getFavorites(int page, int size,
                              FavoritesCallback callback) = 0;

    // ---------- 菜谱相关 ----------
    /**
     * @brief 获取公开菜谱列表（分页，无需认证）
     * @param page 页码
     * @param size 每页数量
     * @param filters 可选筛选条件，格式为 JSON 对象
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getPublicRecipes(int page, int size,
                                  const nlohmann::json& filters,
                                  PagedRecipesCallback callback) = 0;

    /**
     * @brief 智能推荐菜谱（需认证，根据用户偏好和库存）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getRecommendedRecipes(int page, int size,
                                       PagedRecommendedRecipesCallback callback) = 0;

    /**
     * @brief 关键词搜索菜谱（可选认证）
     * @param keyword 搜索关键词
     * @param page 页码
     * @param size 每页数量
     * @param filters 可选筛选条件
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void searchRecipes(const std::string& keyword,
                               int page, int size,
                               const nlohmann::json& filters,
                               PagedRecipesCallback callback) = 0;

    /**
     * @brief 获取菜谱详情（可选认证）
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, detail, error)
     */
    virtual void getRecipeDetail(int recipeId,
                                 RecipeDetailCallback callback) = 0;

    /**
     * @brief 获取菜谱关联视频列表
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, videos, error)
     */
    virtual void getRecipeVideos(int recipeId,
                                 RecipeVideosCallback callback) = 0;

    /**
     * @brief 获取菜谱的评分与评论列表（分页）
     * @param recipeId 菜谱ID
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getRecipeRatings(int recipeId, int page, int size,
                                  PagedRatingsCallback callback) = 0;

    /**
     * @brief 投稿新菜谱（需认证）
     * @param recipeData 菜谱数据
     * @param callback 回调 (success, SubmitRecipeResponse, error)
     */
    virtual void submitRecipe(const gocook::models::SubmitRecipeRequest& recipeData,
                              SubmitRecipeCallback callback) = 0;

    /**
     * @brief 获取当前用户的投稿列表（分页，需认证）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getMySubmittedRecipes(int page, int size,
                                       PagedMyRecipesCallback callback) = 0;

    /**
     * @brief 编辑未审核的菜谱（需认证）
     * @param recipeId 菜谱ID
     * @param updates 更新的字段
     * @param callback 回调 (success, error)
     */
    virtual void editRecipe(int recipeId,
                            const gocook::models::EditRecipeRequest& updates,
                            SuccessCallback callback) = 0;

    /**
     * @brief 切换菜谱收藏状态（需认证）
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, error)
     */
    virtual void toggleFavorite(int recipeId,
                                SuccessCallback callback) = 0;

    /**
     * @brief 评分与评论（需认证）
     * @param recipeId 菜谱ID
     * @param request 评分与评论请求体
     * @param callback 回调 (success, error)
     */
    virtual void rateRecipe(int recipeId,
                            const gocook::models::RateRecipeRequest& request,
                            SuccessCallback callback) = 0;

    // ---------- 库存管理（需认证） ----------
    /**
     * @brief 获取当前用户的库存（分页）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getInventory(int page, int size,
                              PagedInventoryCallback callback) = 0;

    /**
     * @brief 添加/更新库存项（需认证）
     * @param item 库存项
     * @param callback 回调 (success, error)
     */
    virtual void upsertInventory(const gocook::models::UpsertInventoryRequest& item,
                                 SuccessCallback callback) = 0;

    /**
     * @brief 删除库存项（需认证）
     * @param itemId 库存项ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteInventoryItem(int itemId,
                                     SuccessCallback callback) = 0;

    // ---------- 购物清单 / 膳食计划（需认证） ----------
    /**
     * @brief 获取/生成购物清单
     * @param planId 膳食计划ID（可选，为0时根据库存缺口生成）
     * @param callback 回调 (success, shoppingList, error)
     */
    virtual void getShoppingList(int planId,
                                 ShoppingListCallback callback) = 0;

    /**
     * @brief 更新购物清单项状态
     * @param itemId 清单项ID
     * @param request 更新请求（checked）
     * @param callback 回调 (success, error)
     */
    virtual void updateShoppingListItem(int itemId,
                                        const gocook::models::UpdateShoppingItemRequest& request,
                                        SuccessCallback callback) = 0;

    /**
     * @brief 批量添加购物清单项
     * @param items 批量添加请求数组
     * @param callback 回调 (success, result, error)
     */
    virtual void batchAddShoppingItems(const std::vector<gocook::models::BatchShoppingItem>& items,
                                       BatchShoppingCallback callback) = 0;

    // ---------- 膳食计划（需认证） ----------
    /**
     * @brief 创建膳食计划项（需认证）
     * @param planData 计划数据
     * @param callback 回调 (success, planId, error)
     */
    virtual void createMealPlan(const gocook::models::MealPlanRequest& planData,
                                IntCallback callback) = 0;

    /**
     * @brief 获取指定日期范围的膳食计划（需认证）
     * @param startDate 开始日期 (YYYY-MM-DD)
     * @param endDate 结束日期 (YYYY-MM-DD)
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getMealPlans(const std::string& startDate,
                              const std::string& endDate,
                              int page, int size,
                              PagedMealPlansCallback callback) = 0;

    /**
     * @brief 获取膳食计划日历视图详情
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @param callback 回调 (success, calendarData, error)
     */
    virtual void getMealPlanDetail(const std::string& startDate,
                                   const std::string& endDate,
                                   MealPlanCalendarCallback callback) = 0;

    /**
     * @brief 更新膳食计划项
     * @param planId 计划项ID
     * @param updates 可更新字段
     * @param callback 回调 (success, error)
     */
    virtual void updateMealPlan(int planId,
                                const gocook::models::MealPlanRequest& updates,
                                SuccessCallback callback) = 0;

    /**
     * @brief 删除膳食计划项（需认证）
     * @param planId 计划项ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteMealPlan(int planId,
                                SuccessCallback callback) = 0;

    /**
     * @brief 获取营养摄入趋势
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @param callback 回调 (success, trendData, error)
     */
    virtual void getNutritionTrend(const std::string& startDate,
                                   const std::string& endDate,
                                   NutritionTrendCallback callback) = 0;

    // ---------- 公告（公开） ----------
    /**
     * @brief 获取系统公告列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getAnnouncements(int page, int size,
                                  PagedAnnouncementsCallback callback) = 0;

    // ---------- 管理员功能（需管理员权限） ----------
    /**
     * @brief 获取用户列表（管理员）
     * @param page 页码
     * @param size 每页数量
     * @param filters 可选筛选条件
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getUsers(int page, int size,
                          const nlohmann::json& filters,
                          PagedUsersCallback callback) = 0;

    /**
     * @brief 创建用户账户（管理员）
     * @param userData 新用户数据
     * @param callback 回调 (success, error)
     */
    virtual void createUser(const gocook::models::CreateUserRequest& userData,
                            SuccessCallback callback) = 0;

    /**
     * @brief 修改用户信息（管理员）
     * @param userId 用户ID
     * @param updates 可更新字段
     * @param callback 回调 (success, error)
     */
    virtual void updateUser(int userId,
                            const gocook::models::UpdateUserRequest& updates,
                            SuccessCallback callback) = 0;

    /**
     * @brief 冻结/解封用户（管理员）
     * @param userId 用户ID
     * @param request 状态设置请求，包含新的状态字符串
     * @param callback 回调 (success, error)
     */
    virtual void setUserStatus(int userId,
                               const gocook::models::SetUserStatusRequest& request,
                               SuccessCallback callback) = 0;

    /**
     * @brief 删除用户（管理员）
     * @param userId 用户ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteUser(int userId,
                            SuccessCallback callback) = 0;

    /**
     * @brief 获取待审核菜谱列表（管理员）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getPendingRecipes(int page, int size,
                                   PagedPendingRecipesCallback callback) = 0;

    /**
     * @brief 审核通过菜谱（管理员）
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, error)
     */
    virtual void approveRecipe(int recipeId,
                               SuccessCallback callback) = 0;

    /**
     * @brief 审核拒绝菜谱（管理员）
     * @param recipeId 菜谱ID
     * @param request 拒绝请求，包含拒绝原因
     * @param callback 回调 (success, error)
     */
    virtual void rejectRecipe(int recipeId,
                              const gocook::models::RejectRecipeRequest& request,
                              SuccessCallback callback) = 0;

    /**
     * @brief 批量审核菜谱（管理员）
     * @param request 批量审核请求
     * @param callback 回调 (success, result, error)
     */
    virtual void batchReviewRecipes(const gocook::models::BatchReviewRequest& request,
                                    BatchReviewCallback callback) = 0;

    /**
     * @brief 发布系统公告（管理员）
     * @param request 公告请求体
     * @param callback 回调 (success, error)
     */
    virtual void publishAnnouncement(const gocook::models::AnnouncementRequest& request,
                                     SuccessCallback callback) = 0;

    /**
     * @brief 发送系统通知（管理员）
     * @param notification 通知请求体
     * @param callback 回调 (success, result, error)
     */
    virtual void sendNotification(const gocook::models::NotificationRequest& notification,
                                  NotificationCallback callback) = 0;

    // ---------- 令牌管理 ----------
    /**
     * @brief 设置认证令牌（由 AuthManager 调用）
     */
    virtual void setAuthToken(const std::string& token) = 0;

    /**
     * @brief 获取当前认证令牌
     */
    virtual std::string authToken() const = 0;

    // ---------- 未授权回调（替代直接依赖具体实现类的信号） ----------
    /**
     * @brief 注册未授权回调
     *
     * 当 API 实现检测到 401 未授权响应时，应调用此回调。
     * AuthManager 通过此机制获知需要自动登出，而无需知道具体实现类型。
     */
    void setUnauthorizedHandler(std::function<void()> handler) {
        m_unauthorizedHandler = std::move(handler);
    }

protected:
    /**
     * @brief 供子类调用，触发未授权回调
     */
    void invokeUnauthorizedHandler() {
        if (m_unauthorizedHandler) {
            m_unauthorizedHandler();
        }
    }

private:
    std::function<void()> m_unauthorizedHandler;
};