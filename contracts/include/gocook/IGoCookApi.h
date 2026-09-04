#pragma once

#include <string>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>      // 仅用于 filters 参数
#include <gocook/DataModels.h>

// ======================== 回调类型定义 ========================
// 简单成功/失败回调
using SuccessCallback = std::function<void(bool success, const std::string& error)>;

// 登录专用回调
using LoginCallback = std::function<void(bool success,
                                         const gocook::models::LoginResponse& data,
                                         const std::string& error)>;

// 带整数 id 的回调（通常用于创建资源）
using IntCallback = std::function<void(bool success, int id, const std::string& error)>;

// 菜谱投稿专用回调
using SubmitRecipeCallback = std::function<void(bool success,
                                                const gocook::models::SubmitRecipeResponse& data,
                                                const std::string& error)>;

// 用户相关回调
using UserProfileCallback = std::function<void(bool success,
                                               const gocook::models::UserProfile& data,
                                               const std::string& error)>;

using PreferencesCallback = std::function<void(bool success,
                                               const gocook::models::UserPreferences& data,
                                               const std::string& error)>;

using HealthProfileCallback = std::function<void(bool success,
                                                 const gocook::models::HealthProfileResponse& data,
                                                 const std::string& error)>;

using FavoritesCallback = std::function<void(bool success,
                                             const gocook::models::PagedFavorites& data,
                                             const std::string& error)>;

using AvatarUploadCallback = std::function<void(bool success,
                                                const gocook::models::AvatarUploadResponse& data,
                                                const std::string& error)>;

using RecipeImageCallback = std::function<void(bool success,
                                               const std::string& imageUrl,
                                               const std::string& error)>;

// 收藏分组
using FavoriteGroupsCallback = std::function<void(bool success,
                                                  const std::vector<gocook::models::FavoriteGroup>& data,
                                                  const std::string& error)>;

using FavoriteGroupCallback = std::function<void(bool success,
                                                 const gocook::models::FavoriteGroup& data,
                                                 const std::string& error)>;

// 通知
using PagedNotificationsCallback = std::function<void(bool success,
                                                      const gocook::models::PagedNotifications& data,
                                                      const std::string& error)>;

// 我的评论
using PagedUserRatingsCallback = std::function<void(bool success,
                                                    const gocook::models::PagedUserRatings& data,
                                                    const std::string& error)>;

// 菜谱相关回调
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

using MyRecipeRatingCallback = std::function<void(bool success,
                                                   const std::optional<gocook::models::RecipeRating>& data,
                                                   const std::string& error)>;

using PagedMyRecipesCallback = std::function<void(bool success,
                                                  const gocook::models::PagedMyRecipes& data,
                                                  const std::string& error)>;

using NutritionReportCallback = std::function<void(bool success,
                                                   const gocook::models::NutritionReport& data,
                                                   const std::string& error)>;

// 库存管理
using PagedInventoryCallback = std::function<void(bool success,
                                                  const gocook::models::PagedInventory& data,
                                                  const std::string& error)>;

// 购物清单
using ShoppingListCallback = std::function<void(bool success,
                                                const gocook::models::ShoppingList& data,
                                                const std::string& error)>;

using ShoppingListsCallback = std::function<void(bool success,
                                                 const std::vector<gocook::models::ShoppingListSummary>& data,
                                                 const std::string& error)>;

using BatchShoppingCallback = std::function<void(bool success,
                                                 const gocook::models::BatchShoppingResponse& data,
                                                 const std::string& error)>;

// 膳食计划
using MealPlanCalendarCallback = std::function<void(bool success,
                                                    const gocook::models::PagedCalendarDays& data,
                                                    const std::string& error)>;

using NutritionTrendCallback = std::function<void(bool success,
                                                  const gocook::models::NutritionTrendResponse& data,
                                                  const std::string& error)>;

using MealPlansCallback = std::function<void(bool success,
                                             const gocook::models::MealPlansResponse& data,
                                             const std::string& error)>;

// 公告
using PagedAnnouncementsCallback = std::function<void(bool success,
                                                      const gocook::models::PagedAnnouncements& data,
                                                      const std::string& error)>;

// 管理员相关回调
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

using StatisticsCallback = std::function<void(bool success,
                                              const gocook::models::StatisticsData& data,
                                              const std::string& error)>;

using PagedAdminLogsCallback = std::function<void(bool success,
                                                  const gocook::models::PagedAdminLogs& data,
                                                  const std::string& error)>;

using PagedActivityLogsCallback = std::function<void(bool success,
                                                     const gocook::models::PagedActivityLogs& data,
                                                     const std::string& error)>;

/**
 * @brief 客户端 API 抽象接口（服务端 REST API 的客户端门面），
 *        定义客户端与后端交互所需的全部异步方法。
 *
 * 具体实现类（HttpGoCookApi）负责通过 HTTP 发送请求。
 * 客户端高层模块（ViewModel）只依赖本接口，符合依赖倒置原则；
 * 服务端业务抽象见 IServices.h（由 server/services 下的 *ServiceImpl 实现）。
 * 所有业务数据均使用强类型结构体（定义于 DataModels.h），
 * 仅筛选条件 filters 因结构多变暂时保留为 nlohmann::json。
 */
class IGoCookApi {
public:
    virtual ~IGoCookApi() = default;

    // ---------- 认证（公开接口） ----------
    /**
     * @brief 用户注册
     * @param request 注册请求，包含用户名、密码和邮箱
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

    // ---------- 忘记密码（公开接口） ----------
    /**
     * @brief 发送密码重置邮件
     * @param email 注册邮箱
     * @param callback 回调 (success, error)，无论成功与否统一返回成功信息（防枚举）
     */
    virtual void forgotPassword(const std::string& username,
                                const std::string& email,
                                SuccessCallback callback) = 0;

    /**
     * @brief 重置密码
     * @param token 邮件中的重置令牌
     * @param newPassword 新密码
     * @param callback 回调 (success, error)
     */
    virtual void resetPassword(const std::string& token,
                               const std::string& newPassword,
                               SuccessCallback callback) = 0;

    // ---------- 用户相关（需认证） ----------
    /**
     * @brief 获取当前登录用户信息
     * @param callback 回调 (success, profile, error)
     */
    virtual void getCurrentUser(UserProfileCallback callback) = 0;

    /**
     * @brief 更新当前用户个人资料
     * @param profile 用户资料，可部分更新
     * @param callback 回调 (success, updatedProfile, error) 返回最新用户信息
     */
    virtual void updateProfile(const gocook::models::UpdateProfileRequest& profile,
                               UserProfileCallback callback) = 0;

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
     * @brief 获取当前用户的健康指标
     * @param callback 回调 (success, healthProfile, error)
     */
    virtual void getHealthProfile(HealthProfileCallback callback) = 0;

    /**
     * @brief 上传头像
     * @param filePath 本地图片文件路径
     * @param callback 回调 (success, AvatarUploadResponse, error)
     */
    virtual void uploadAvatar(const std::string& filePath,
                              AvatarUploadCallback callback) = 0;

    /**
     * @brief 上传菜谱封面图片
     * @param recipeId 菜谱 ID
     * @param filePath 本地图片文件路径
     * @param callback 回调 (success, imageUrl, error)
     */
    virtual void uploadRecipeImage(int recipeId,
                                   const std::string& filePath,
                                   RecipeImageCallback callback) = 0;

    /**
     * @brief 上传菜谱步骤图片
     * @param recipeId 菜谱 ID
     * @param stepIndex 步骤索引（0-based）
     * @param filePath 本地图片文件路径
     * @param callback 回调 (success, error)
     */
    virtual void uploadStepImage(int recipeId, int stepIndex,
                                 const std::string& filePath,
                                 RecipeImageCallback callback) = 0;

    /**
     * @brief 删除待审核菜谱
     */
    virtual void deleteRecipe(int recipeId, SuccessCallback callback) = 0;

    /**
     * @brief 修改密码（需验证原密码）
     * @param currentPassword 当前密码
     * @param newPassword 新密码
     * @param callback 回调 (success, error)
     */
    virtual void changePassword(const std::string& currentPassword,
                                const std::string& newPassword,
                                SuccessCallback callback) = 0;

    /**
     * @brief 注销账户
     * @param callback 回调 (success, error)
     */
    virtual void deleteAccount(SuccessCallback callback) = 0;

    /**
     * @brief 获取当前用户的收藏列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @param group 可选分组名
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getFavorites(int page, int size,
                              const std::string& group,
                              FavoritesCallback callback) = 0;

    // ---------- 收藏分组管理（需认证） ----------
    /**
     * @brief 获取收藏分组列表
     * @param callback 回调 (success, groups, error)
     */
    virtual void getFavoriteGroups(FavoriteGroupsCallback callback) = 0;

    /**
     * @brief 创建收藏分组
     * @param request 分组名
     * @param callback 回调 (success, group, error) 返回创建的分组对象
     */
    virtual void createFavoriteGroup(const gocook::models::CreateGroupRequest& request,
                                     FavoriteGroupCallback callback) = 0;

    /**
     * @brief 更新收藏分组名称
     * @param groupId 分组ID
     * @param request 新名称
     * @param callback 回调 (success, error)
     */
    virtual void updateFavoriteGroup(int groupId,
                                     const gocook::models::UpdateGroupRequest& request,
                                     SuccessCallback callback) = 0;

    /**
     * @brief 删除收藏分组
     * @param groupId 分组ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteFavoriteGroup(int groupId,
                                     SuccessCallback callback) = 0;

    /**
     * @brief 更新收藏项属性（分组、可见性）
     * @param favoriteId 收藏项ID
     * @param request 更新内容
     * @param callback 回调 (success, error)
     */
    virtual void updateFavoriteItem(int favoriteId,
                                    const gocook::models::UpdateFavoriteRequest& request,
                                    SuccessCallback callback) = 0;

    /**
     * @brief 批量删除收藏
     * @param request 包含要删除的 favorite_ids
     * @param callback 回调 (success, error)
     */
    virtual void batchDeleteFavorites(const gocook::models::BatchDeleteFavoritesRequest& request,
                                      SuccessCallback callback) = 0;

    // ---------- 通知中心（需认证） ----------
    /**
     * @brief 获取通知列表（分页，可选按类型筛选）
     * @param page 页码
     * @param size 每页数量
     * @param type 可选通知类型，传空字符串表示不过滤
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getNotifications(int page, int size,
                                  const std::string& type,
                                  PagedNotificationsCallback callback) = 0;

    /**
     * @brief 标记单条通知已读
     * @param notificationId 通知ID
     * @param callback 回调 (success, error)
     */
    virtual void markNotificationRead(int notificationId,
                                      SuccessCallback callback) = 0;

    /**
     * @brief 全部标记已读
     * @param callback 回调 (success, error)
     */
    virtual void markAllNotificationsRead(SuccessCallback callback) = 0;

    /**
     * @brief 删除通知
     * @param notificationId 通知ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteNotification(int notificationId,
                                    SuccessCallback callback) = 0;

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
     * @brief 获取当前用户对某个菜谱的评分（需认证）
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, optionalRating, error)
     */
    virtual void getMyRecipeRating(int recipeId,
                                   MyRecipeRatingCallback callback) = 0;

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
     * @param status 可选，按状态筛选，传空字符串表示返回全部
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getMySubmittedRecipes(int page, int size,
                                       const std::string& status,
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
     * @param groupId 可选，目标分组ID
     * @param isPublic 可选，可见性
     * @param callback 回调 (success, error)
     */
    virtual void toggleFavorite(int recipeId,
                                std::optional<int> groupId,
                                std::optional<bool> isPublic,
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

    /**
     * @brief 修改评论（需认证）
     * @param recipeId 菜谱ID
     * @param ratingId 评论ID
     * @param request 修改内容
     * @param callback 回调 (success, error)
     */
    virtual void updateRating(int recipeId, int ratingId,
                              const gocook::models::RateRecipeRequest& request,
                              SuccessCallback callback) = 0;

    /**
     * @brief 删除评论（需认证）
     * @param recipeId 菜谱ID
     * @param ratingId 评论ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteRating(int recipeId, int ratingId,
                              SuccessCallback callback) = 0;

    /**
     * @brief 获取当前用户的所有评论列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getMyRatings(int page, int size,
                              PagedUserRatingsCallback callback) = 0;

    /**
     * @brief 获取独立营养报告
     * @param recipeId 菜谱ID
     * @param callback 回调 (success, NutritionReport, error)
     */
    virtual void getRecipeNutrition(int recipeId,
                                    NutritionReportCallback callback) = 0;

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
     * @brief 添加库存项（需认证；同名同单位自动累加，不同单位新增行）
     * @param item 库存项
     * @param callback 回调 (success, id, error) 返回库存项 ID
     */
    virtual void upsertInventory(const gocook::models::UpsertInventoryRequest& item,
                                 IntCallback callback) = 0;

    /**
     * @brief 编辑库存项（需认证；PUT /api/inventory/:id，按 id 整行替换）
     * @param itemId 库存项 ID
     * @param item 替换后的库存项数据
     * @param callback 回调 (success, error)
     */
    virtual void updateInventoryItem(int itemId,
                                     const gocook::models::UpsertInventoryRequest& item,
                                     SuccessCallback callback) = 0;

    /**
     * @brief 删除库存项（需认证）
     * @param itemId 库存项ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteInventoryItem(int itemId,
                                     SuccessCallback callback) = 0;

    // ---------- 购物清单（多清单模型，需认证） ----------
    /**
     * @brief 获取当前用户的购物清单列表
     * @param callback 回调 (success, list of summaries, error)
     */
    virtual void getShoppingLists(ShoppingListsCallback callback) = 0;

    /**
     * @brief 创建购物清单
     * @param request 清单名称与可选计划ID
     * @param callback 回调 (success, ShoppingList, error) 返回包含完整信息的清单对象
     */
    virtual void createShoppingList(const gocook::models::CreateShoppingListRequest& request,
                                    ShoppingListCallback callback) = 0;

    /**
     * @brief 获取购物清单详情
     * @param listId 清单ID
     * @param callback 回调 (success, ShoppingList, error)
     */
    virtual void getShoppingListDetail(int listId,
                                       ShoppingListCallback callback) = 0;

    /**
     * @brief 删除购物清单
     * @param listId 清单ID
     * @param callback 回调 (success, error)
     */
    virtual void deleteShoppingList(int listId,
                                    SuccessCallback callback) = 0;

    /**
     * @brief 更新购物清单项状态
     * @param listId 清单ID
     * @param itemId 清单项ID
     * @param request 更新请求（checked）
     * @param callback 回调 (success, error)
     */
    virtual void updateShoppingListItem(int listId, int itemId,
                                        const gocook::models::UpdateShoppingItemRequest& request,
                                        SuccessCallback callback) = 0;

    /**
     * @brief 批量添加购物清单项
     * @param listId 清单ID
     * @param items 批量添加请求数组
     * @param callback 回调 (success, result, error)
     */
    virtual void batchAddShoppingItems(int listId,
                                       const std::vector<gocook::models::BatchShoppingItem>& items,
                                       BatchShoppingCallback callback) = 0;

    /**
     * @brief 导出购物清单
     * @param listId 清单ID
     * @param format 导出格式 "text" 或 "image"
     * @param callback 回调 (success, dataOrPath, error)
     *                 第二个参数：当 format="text" 时为纯文本内容；
     *                 当 format="image" 时为图片的 base64 编码字符串。
     */
    virtual void exportShoppingList(int listId,
                                    const std::string& format,
                                    std::function<void(bool, const std::string&, const std::string&)> callback) = 0;

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
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @param page 页码
     * @param size 每页数量
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getMealPlans(const std::string& startDate,
                              const std::string& endDate,
                              int page, int size,
                              MealPlansCallback callback) = 0;

    /**
     * @brief 获取膳食计划日历视图详情
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @param page 页码（每页天数）
     * @param size 每页天数
     * @param callback 回调 (success, calendarData（含分页）, error)
     */
    virtual void getMealPlanDetail(const std::string& startDate,
                                   const std::string& endDate,
                                   int page, int size,
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
     * @param request 状态设置请求
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

    /**
     * @brief 查看运营数据统计（管理员）
     * @param callback 回调 (success, StatisticsData, error)
     */
    virtual void getStatistics(StatisticsCallback callback) = 0;

    /**
     * @brief 查询管理员操作日志（管理员）
     * @param page 页码
     * @param size 每页数量
     * @param type 可选日志类型筛选，传空字符串表示不过滤
     * @param userId 可选操作者ID筛选，传0表示不过滤
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getAdminLogs(int page, int size,
                              const std::string& type,
                              int userId,
                              PagedAdminLogsCallback callback) = 0;

    /**
     * @brief 查询全局用户行为日志（管理员）
     * @param page 页码
     * @param size 每页数量
     * @param userId 可选用户ID筛选，传0表示不过滤
     * @param action 可选行为类型筛选，传空字符串表示不过滤
     * @param callback 回调 (success, pagedResult, error)
     */
    virtual void getActivityLogs(int page, int size,
                                 int userId,
                                 const std::string& action,
                                 PagedActivityLogsCallback callback) = 0;

    // ---------- 测试辅助（调试用） ----------
    /**
     * @brief 重置测试用户的通知数据（仅开发环境可用）
     */
    virtual void resetTestNotifications(SuccessCallback callback) = 0;

    // ---------- 令牌管理 ----------
    /**
     * @brief 设置认证令牌（由 AuthManager 调用）
     */
    virtual void setAuthToken(const std::string& token) = 0;

    /**
     * @brief 获取当前认证令牌
     */
    virtual std::string authToken() const = 0;

    /**
     * @brief 设置 401 未授权回调（由前端 ViewModel 注入）
     */
    virtual void setUnauthorizedHandler(std::function<void()> handler) = 0;

};