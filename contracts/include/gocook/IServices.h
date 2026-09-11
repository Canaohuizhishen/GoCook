#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>
#include <string>

/**
 * @brief 服务端业务 API 抽象接口，定义各业务域的服务方法（同步返回 DTO）。
 *
 * 具体实现类（*ServiceImpl）位于 server/services/，被 server/handlers/ 调用，
 * 经 Router 转成 HTTP 路由对外提供服务。
 * 客户端侧对应 IGoCookApi.h 的异步 API 门面（HttpGoCookApi 经 HTTP 请求本层）。
 * 所有业务数据均使用强类型结构体（定义于 DataModels.h）。
 */
namespace gocook::services {

    /**
     * @brief 自定义业务异常，由 Handler 转换为 HTTP 状态码。
     *
     * 服务层抛出该异常后，Handler 依据 statusCode 映射对应的 HTTP 响应。
     */
    class ServiceException : public std::runtime_error {
    public:
        /**
         * @brief 构造函数。
         * @param msg 异常描述（中文）
         * @param statusCode 对应的 HTTP 状态码（默认 500）
         */
        ServiceException(const std::string& msg, int statusCode = 500)
            : std::runtime_error(msg), statusCode_(statusCode) {}
        /// 返回对应的 HTTP 状态码
        int statusCode() const { return statusCode_; }
    private:
        int statusCode_;   ///< HTTP 状态码
    };

    // ======================== 菜谱服务接口 ========================
    /**
     * @brief 菜谱服务抽象接口，定义菜谱域的全部业务方法。
     *
     * 由 server/services/RecipeServiceImpl 实现，供 RecipeHandler 调用。
     */
    class IRecipeService {
    public:
        virtual ~IRecipeService() = default;

        /**
         * @brief 获取公开菜谱列表（无需认证）。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param filters 筛选条件（JSON 对象，如技法/口味/食材类型等）
         * @return 分页的菜谱摘要列表
         */
        virtual models::PagedRecipes getPublicRecipes(int page, int size,
                                                      const nlohmann::json& filters) = 0;

        /**
         * @brief 按关键词搜索菜谱（分页）。
         * @param keyword 搜索关键词
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param filters 筛选条件（JSON 对象）
         * @return 分页的菜谱摘要列表
         */
        virtual models::PagedRecipes searchRecipes(const std::string& keyword,
                                                   int page, int size,
                                                   const nlohmann::json& filters) = 0;

        /**
         * @brief 智能推荐菜谱（需用户偏好和库存）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的推荐菜谱（含匹配度与库存匹配详情）
         */
        virtual models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                      int page, int size) = 0;

        /**
         * @brief 获取菜谱详情（可传 userId 查询当前用户收藏状态）。
         * @param recipeId 菜谱 ID
         * @param userId 当前用户 ID，0 表示未登录（默认）
         * @return 菜谱详情
         */
        virtual models::RecipeDetail getRecipeDetail(int recipeId, int userId = 0) = 0;

        /**
         * @brief 获取菜谱关联视频列表。
         * @param recipeId 菜谱 ID
         * @return 视频列表
         */
        virtual std::vector<models::RecipeVideo> getRecipeVideos(int recipeId) = 0;

        /**
         * @brief 获取菜谱评分与评论（分页）。
         * @param recipeId 菜谱 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的评分评论列表
         */
        virtual models::PagedRatings getRecipeRatings(int recipeId, int page, int size) = 0;

        /**
         * @brief 投稿新菜谱（需认证）。
         * @param userId 投稿用户 ID
         * @param data 投稿数据（菜谱名/食材/步骤等）
         * @return 投稿响应（含新菜谱 ID 与状态）
         */
        virtual models::SubmitRecipeResponse submitRecipe(int userId,
                                                          const models::SubmitRecipeRequest& data) = 0;

        /**
         * @brief 获取我的投稿列表（需认证，可选按状态筛选）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param status 可选筛选状态："pending", "approved", "rejected"，空字符串表示全部
         * @return 分页的投稿列表
         */
        virtual models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                             int page, int size,
                                                             const std::string& status = "") = 0;

        /**
         * @brief 编辑未审核的菜谱（需认证）。
         * @param userId 操作者用户 ID
         * @param recipeId 菜谱 ID
         * @param updates 待更新的字段
         * @return 更新后的状态（"pending"）
         */
        virtual std::string editRecipe(int userId, int recipeId,
                                         const models::EditRecipeRequest& updates) = 0;

        /**
         * @brief 切换收藏状态（需认证）。
         * @param userId 用户 ID
         * @param recipeId 菜谱 ID
         * @param groupId 可选分组ID，不传则使用默认分组
         * @param isPublic 可选可见性，nullopt 表示保持默认或当前状态
         */
        virtual void toggleFavorite(int userId, int recipeId,
                                    std::optional<int> groupId = std::nullopt,
                                    std::optional<bool> isPublic = std::nullopt) = 0;

        /**
         * @brief 评分与评论（需认证）。
         * @param userId 评分用户 ID
         * @param recipeId 菜谱 ID
         * @param request 评分（1-5）与评论内容
         */
        virtual void rateRecipe(int userId, int recipeId,
                                const models::RateRecipeRequest& request) = 0;

        /**
         * @brief 修改评论（需认证）。
         * @param userId 评论作者用户 ID
         * @param recipeId 菜谱 ID
         * @param ratingId 评论 ID
         * @param request 新的评分与评论内容
         */
        virtual void updateRating(int userId, int recipeId, int ratingId,
                                  const models::RateRecipeRequest& request) = 0;

        /**
         * @brief 删除评论（需认证）。
         * @param userId 评论作者用户 ID
         * @param recipeId 菜谱 ID
         * @param ratingId 评论 ID
         */
        virtual void deleteRating(int userId, int recipeId, int ratingId) = 0;

        /**
         * @brief 获取当前用户对某个菜谱的评分（不存在返回 nullopt）。
         * @param userId 用户 ID
         * @param recipeId 菜谱 ID
         * @return 评分记录；未评分时返回 nullopt
         */
        virtual std::optional<models::RecipeRating> getMyRating(int userId,
                                                                 int recipeId) = 0;

        /**
         * @brief 获取当前用户的所有评论列表（分页）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的评论列表
         */
        virtual models::PagedUserRatings getMyRatings(int userId, int page, int size) = 0;

        /**
         * @brief 获取独立营养报告（对应 API 4.15）。
         * @param recipeId 菜谱 ID
         * @return 营养报告（每份营养/食材明细/健康提示）
         */
        virtual models::NutritionReport getRecipeNutrition(int recipeId) = 0;

        /**
         * @brief 更新菜谱封面图片：上传图片并返回 image_url。
         * @param recipeId 菜谱 ID
         * @param filePath 本地图片文件路径
         * @return 可访问的 image_url
         */
        virtual std::string uploadRecipeImage(int recipeId,
                                               const std::string& filePath) = 0;

        /**
         * @brief 更新菜谱某一步骤的图片，返回 image_url。
         * @param recipeId 菜谱 ID
         * @param stepIndex 步骤索引（从 0 开始）
         * @param filePath 本地图片文件路径
         * @return 可访问的 image_url
         */
        virtual std::string uploadStepImage(int recipeId, int stepIndex,
                                             const std::string& filePath) = 0;

        /**
         * @brief 删除待审核菜谱（仅非 approved 状态可删）。
         * @param userId 操作者用户 ID
         * @param recipeId 菜谱 ID
         */
        virtual void deleteRecipe(int userId, int recipeId) = 0;
    };

    // ======================== 用户服务接口 ========================
    /**
     * @brief 用户服务抽象接口，定义用户域的全部业务方法。
     *
     * 由 server/services/UserServiceImpl 实现，供 UserHandler 调用。
     */
    class IUserService {
    public:
        virtual ~IUserService() = default;

        /**
         * @brief 用户注册（无需认证；两段式注册第一步：写入待验证记录并发送验证码邮件，不直接建号）。
         * @param request 注册请求（用户名/密码/邮箱）
         */
        virtual void registerUser(const models::RegisterRequest& request) = 0;

        /**
         * @brief 完成注册（两段式注册第二步：邮箱验证码核验后建号）。
         * @param email 注册邮箱
         * @param token 邮件中的 6 位数字验证码
         */
        virtual void verifyRegistration(const std::string& email,
                                        const std::string& token) = 0;

        /**
         * @brief 用户登录，返回 token 等信息（无需认证）。
         * @param request 登录请求（用户名/密码）
         * @return 登录响应（含 JWT 令牌）
         */
        virtual models::LoginResponse login(const models::LoginRequest& request) = 0;

        /**
         * @brief 发送密码重置邮件（无需认证，对应 API 3.11.2）。
         * @param username 用户名（用于双重验证）
         * @param email 注册邮箱
         * @return SMTP 未配置时返回重置令牌（开发模式），邮件成功发送时返回 std::nullopt
         */
        virtual std::optional<std::string> requestPasswordReset(
            const std::string& username, const std::string& email) = 0;

        /**
         * @brief 重置密码（无需认证）。
         * @param token 邮件中的重置令牌
         * @param newPassword 新密码
         */
        virtual void resetPassword(const std::string& token,
                                   const std::string& newPassword) = 0;

        /**
         * @brief 获取当前用户信息（需认证）。
         * @param userId 用户 ID
         * @return 用户公开资料
         */
        virtual models::UserProfile getCurrentUser(int userId) = 0;

        /**
         * @brief 更新当前用户个人资料（需认证），返回更新后的资料。
         * @param userId 用户 ID
         * @param profile 待更新的资料字段（可选字段为空表示不修改）
         * @return 更新后的用户资料
         */
        virtual models::UserProfile updateProfile(int userId,
                                                  const models::UpdateProfileRequest& profile) = 0;

        /**
         * @brief 修改密码（需认证）。
         * @param userId 用户 ID
         * @param currentPassword 当前密码
         * @param newPassword 新密码
         */
        virtual void changePassword(int userId,
                                    const std::string& currentPassword,
                                    const std::string& newPassword) = 0;

        /**
         * @brief 注销账户（需认证）。
         * @param userId 用户 ID
         */
        virtual void deleteAccount(int userId) = 0;

        /**
         * @brief 上传头像，返回 URL 和 ID（需认证）。
         * @param userId 用户 ID
         * @param filePath 本地图片文件路径
         * @return 头像响应（avatar_id 与 avatar_url）
         */
        virtual models::AvatarUploadResponse uploadAvatar(int userId,
                                                          const std::string& filePath) = 0;

        /**
         * @brief 获取用户饮食偏好（需认证）。
         * @param userId 用户 ID
         * @return 偏好数据（喜爱/禁忌/健康目标）
         */
        virtual models::UserPreferences getPreferences(int userId) = 0;

        /**
         * @brief 更新用户饮食偏好（需认证）。
         * @param userId 用户 ID
         * @param prefs 新的偏好数据
         */
        virtual void updatePreferences(int userId,
                                       const models::UserPreferences& prefs) = 0;

        /**
         * @brief 录入/更新健康指标（需认证）。
         * @param userId 用户 ID
         * @param healthProfile 身高体重与健康条件
         * @return 更新后的健康指标（含忌口建议）
         */
        virtual models::HealthProfileResponse updateHealthProfile(
            int userId, const models::HealthProfileRequest& healthProfile) = 0;
        /**
         * @brief 获取健康指标（需认证）。
         * @param userId 用户 ID
         * @return 健康指标（含忌口建议）
         */
        virtual models::HealthProfileResponse getHealthProfile(int userId) = 0;

        /**
         * @brief 获取用户收藏列表（分页，需认证，可选按分组筛选）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param group 可选分组名，空字符串表示所有分组
         * @return 分页的收藏列表
         */
        virtual models::PagedFavorites getFavorites(int userId, int page, int size,
                                                    const std::string& group = "") = 0;

        /**
         * @brief 获取收藏分组列表。
         * @param userId 用户 ID
         * @return 分组列表（含各自收藏数量）
         */
        virtual std::vector<models::FavoriteGroup> getFavoriteGroups(int userId) = 0;

        /**
         * @brief 创建收藏分组，返回创建的分组对象。
         * @param userId 用户 ID
         * @param request 分组名
         * @return 创建的分组对象
         */
        virtual models::FavoriteGroup createFavoriteGroup(int userId,
                                                          const models::CreateGroupRequest& request) = 0;

        /**
         * @brief 更新收藏分组。
         * @param userId 用户 ID
         * @param groupId 分组 ID
         * @param request 新分组名
         */
        virtual void updateFavoriteGroup(int userId, int groupId,
                                         const models::UpdateGroupRequest& request) = 0;

        /**
         * @brief 删除收藏分组。
         * @param userId 用户 ID
         * @param groupId 分组 ID
         */
        virtual void deleteFavoriteGroup(int userId, int groupId) = 0;

        /**
         * @brief 更新收藏项属性。
         * @param userId 用户 ID
         * @param favoriteId 收藏记录 ID
         * @param request 待更新的属性（分组/可见性）
         */
        virtual void updateFavoriteItem(int userId, int favoriteId,
                                        const models::UpdateFavoriteRequest& request) = 0;

        /**
         * @brief 批量删除收藏。
         * @param userId 用户 ID
         * @param request 待删除的收藏 ID 列表
         */
        virtual void batchDeleteFavorites(int userId,
                                          const models::BatchDeleteFavoritesRequest& request) = 0;

        /**
         * @brief 获取通知列表（分页，可选按类型筛选）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param type 可选通知类型："system", "review", "interaction"，空字符串表示所有
         * @return 分页的通知列表
         */
        virtual models::PagedNotifications getNotifications(int userId, int page, int size,
                                                            const std::string& type = "") = 0;

        /**
         * @brief 标记通知已读。
         * @param userId 用户 ID
         * @param notificationId 通知 ID
         */
        virtual void markNotificationRead(int userId, int notificationId) = 0;

        /**
         * @brief 全部标记已读。
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

    // ======================== 库存与购物清单服务接口 ========================
    /**
     * @brief 库存与购物清单服务抽象接口，定义该域的全部业务方法。
     *
     * 由 server/services/InventoryServiceImpl 实现，供 InventoryHandler 调用。
     */
    class IInventoryService {
    public:
        virtual ~IInventoryService() = default;

        /**
         * @brief 获取当前用户库存（分页，需认证）。
         * @param userId 用户 ID
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param keyword 食材名模糊过滤词（v2.14：空串 = 不过滤）
         * @return 分页的库存列表
         *
         * 注：接口声明不设默认实参（纯虚接口默认参易与实现侧漂移），调用须显式传 keyword，
         * 空串 = 不过滤；InventoryServiceImpl 实现侧保留默认实参供直接调用方省略。
         */
        virtual models::PagedInventory getInventory(int userId, int page, int size,
                                                    const std::string& keyword) = 0;

        /**
         * @brief 添加库存项（需认证；同名同单位数量累加、不同单位新增行，带 expiry_date 的
         *        追加视作新批次混入，行内到期日取最早），返回库存项 ID。
         * @param userId 用户 ID
         * @param item 库存项数据
         * @return 库存项 ID
         */
        virtual int upsertInventory(int userId,
                                    const models::UpsertInventoryRequest& item) = 0;

        /**
         * @brief 编辑库存项（按 id 整行替换，需认证；对应 PUT /api/inventory/:id）。
         *        item 缺省 expiry_date = 清空该列；与本人另一条 (user_id, ingredient_name, unit)
         *        重复时抛 ServiceException(409)。
         * @param userId 用户 ID
         * @param itemId 库存项 ID（须属于该用户，否则 404）
         * @param item 替换后的库存项数据
         */
        virtual void updateInventoryItem(int userId, int itemId,
                                         const models::UpsertInventoryRequest& item) = 0;

        /**
         * @brief 删除库存项（需认证）。
         * @param userId 用户 ID
         * @param itemId 库存项 ID
         */
        virtual void deleteInventoryItem(int userId, int itemId) = 0;

        // ---------- 购物清单管理（多清单模型） ----------

        /**
         * @brief 获取用户的购物清单列表。
         * @param userId 用户 ID
         * @return 清单摘要列表
         */
        virtual std::vector<models::ShoppingListSummary> getShoppingLists(int userId) = 0;

        /**
         * @brief 创建购物清单，返回新清单 ID。
         * 若请求中提供 plan_id，则基于该膳食计划自动生成清单内容。
         * @param userId 用户 ID
         * @param request 清单名与可选的膳食计划 plan_id
         * @return 新清单 ID
         */
        virtual int createShoppingList(int userId,
                                       const models::CreateShoppingListRequest& request) = 0;

        /**
         * @brief 获取指定购物清单详情。
         * @param userId 用户 ID
         * @param listId 清单 ID
         * @return 完整清单（含条目）
         */
        virtual models::ShoppingList getShoppingListDetail(int userId, int listId) = 0;

        /**
         * @brief 删除购物清单。
         * @param userId 用户 ID
         * @param listId 清单 ID
         */
        virtual void deleteShoppingList(int userId, int listId) = 0;

        /**
         * @brief 更新购物清单项状态（需传入 listId 和 itemId）。
         * @param userId 用户 ID
         * @param listId 清单 ID
         * @param itemId 清单项 ID
         * @param request 待更新的状态
         */
        virtual void updateShoppingListItem(int userId, int listId, int itemId,
                                            const models::UpdateShoppingItemRequest& request) = 0;

        /**
         * @brief 批量添加购物清单项（需传入 listId）。
         * @param userId 用户 ID
         * @param listId 清单 ID
         * @param items 待添加的条目列表
         * @return 处理结果（成功消息与完整条目列表）
         */
        virtual models::BatchShoppingResponse batchAddShoppingItems(
            int userId, int listId, const std::vector<models::BatchShoppingItem>& items) = 0;

        /**
         * @brief 导出购物清单，返回内容（文本为纯文本，图片为 base64 编码）。
         * @param userId 用户 ID
         * @param listId 清单 ID
         * @param format "text" 或 "image"
         * @return 导出内容
         */
        virtual std::string exportShoppingList(int userId, int listId,
                                               const std::string& format) = 0;
    };

    // ======================== 膳食计划服务接口 ========================
    /**
     * @brief 膳食计划服务抽象接口，定义膳食计划域的全部业务方法。
     *
     * 由 server/services/MealPlanServiceImpl 实现，供 MealPlanHandler 调用。
     */
    class IMealPlanService {
    public:
        virtual ~IMealPlanService() = default;

        /**
         * @brief 创建膳食计划项（需认证），返回计划项 ID。
         * @param userId 用户 ID
         * @param planData 计划数据（菜谱/日期/餐次）
         * @return 计划项 ID
         */
        virtual int createMealPlan(int userId,
                                   const models::MealPlanRequest& planData) = 0;

        /**
         * @brief 获取膳食计划列表（需认证）。
         * @param userId 用户 ID
         * @param startDate 开始日期（YYYY-MM-DD）
         * @param endDate 结束日期（YYYY-MM-DD）
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的膳食计划列表（含营养汇总）
         */
        virtual models::MealPlansResponse getMealPlans(int userId,
                                                       const std::string& startDate,
                                                       const std::string& endDate,
                                                       int page, int size) = 0;

        /**
         * @brief 获取膳食计划日历视图详情（需认证），支持分页。
         * @param userId 用户 ID
         * @param startDate 开始日期（YYYY-MM-DD）
         * @param endDate 结束日期（YYYY-MM-DD）
         * @param page 页码，每页返回的天数
         * @param size 每页天数，最大 30
         * @return 分页的日历天数列表（含每日营养合计）
         */
        virtual models::PagedCalendarDays getMealPlanDetail(int userId,
                                                            const std::string& startDate,
                                                            const std::string& endDate,
                                                            int page, int size) = 0;

        /**
         * @brief 更新膳食计划项（需认证）。
         * @param userId 用户 ID
         * @param planId 计划项 ID
         * @param updates 待更新的字段
         */
        virtual void updateMealPlan(int userId, int planId,
                                    const models::MealPlanRequest& updates) = 0;

        /**
         * @brief 删除膳食计划项（需认证）。
         * @param userId 用户 ID
         * @param planId 计划项 ID
         */
        virtual void deleteMealPlan(int userId, int planId) = 0;

        /**
         * @brief 获取营养摄入趋势（需认证）。
         * @param userId 用户 ID
         * @param startDate 开始日期（YYYY-MM-DD）
         * @param endDate 结束日期（YYYY-MM-DD）
         * @return 趋势数据（每日实际摄入对比推荐目标）
         */
        virtual models::NutritionTrendResponse getNutritionTrend(int userId,
                                                                 const std::string& startDate,
                                                                 const std::string& endDate) = 0;
    };

    // ======================== 公告服务接口（公开） ========================
    /**
     * @brief 公告服务抽象接口，定义公告域的全部业务方法。
     *
     * 由 server/services/AnnouncementServiceImpl 实现，供 AnnouncementHandler 调用。
     */
    class IAnnouncementService {
    public:
        virtual ~IAnnouncementService() = default;

        /**
         * @brief 获取系统公告列表（分页，无需认证）。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的公告列表
         */
        virtual models::PagedAnnouncements getAnnouncements(int page, int size) = 0;
    };

    // ======================== 管理员服务接口 ========================
    /**
     * @brief 管理员服务抽象接口，定义后台管理域的全部业务方法。
     *
     * 由 server/services/AdminServiceImpl 实现，供 AdminHandler 调用。
     */
    class IAdminService {
    public:
        virtual ~IAdminService() = default;

        /**
         * @brief 获取用户列表（管理员）。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param filters 筛选条件（JSON 对象，如角色/状态/关键字）
         * @return 分页的管理端用户列表
         */
        virtual models::PagedUsers getUsers(int page, int size,
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
         * @param request 目标状态："active" 或 "frozen"
         */
        virtual void setUserStatus(int userId,
                                   const models::SetUserStatusRequest& request) = 0;

        /**
         * @brief 删除用户（管理员）。
         * @param userId 用户 ID
         */
        virtual void deleteUser(int userId) = 0;

        /**
         * @brief 获取待审核菜谱列表（管理员）。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @return 分页的待审核菜谱列表
         */
        virtual models::PagedPendingRecipes getPendingRecipes(int page, int size) = 0;

        /**
         * @brief 审核通过菜谱（管理员）。
         * @param recipeId 菜谱 ID
         */
        virtual void approveRecipe(int recipeId) = 0;

        /**
         * @brief 审核拒绝菜谱（管理员）。
         * @param recipeId 菜谱 ID
         * @param request 拒绝原因
         */
        virtual void rejectRecipe(int recipeId,
                                  const models::RejectRecipeRequest& request) = 0;

        /**
         * @brief 批量审核菜谱（管理员）。
         * @param request 待审核菜谱 ID 列表与动作（approve/reject）
         * @return 处理结果（成功数量与失败 ID 列表）
         */
        virtual models::BatchReviewResponse batchReviewRecipes(
            const models::BatchReviewRequest& request) = 0;

        /**
         * @brief 发布系统公告（管理员）。
         * @param request 公告标题与内容
         */
        virtual void publishAnnouncement(const models::AnnouncementRequest& request) = 0;

        /**
         * @brief 发送系统通知（管理员）。
         * @param notification 通知内容与目标
         * @return 通知发送响应（ID/状态/预计接收人数）
         */
        virtual models::NotificationResponse sendNotification(
            const models::NotificationRequest& notification) = 0;

        /**
         * @brief 获取运营数据统计。
         * @return 统计快照（用户/菜谱/审核/增长等）
         */
        virtual models::StatisticsData getStatistics() = 0;

        /**
         * @brief 查询管理员操作日志。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param type 可选日志类型筛选
         * @param userId 可选操作者ID筛选
         * @return 分页的管理员操作日志
         */
        virtual models::PagedAdminLogs getAdminLogs(int page, int size,
                                                    const std::string& type = "",
                                                    int userId = 0) = 0;

        /**
         * @brief 查询全局用户行为日志。
         * @param page 页码（从 1 开始）
         * @param size 每页数量
         * @param userId 可选用户ID筛选
         * @param action 可选行为类型筛选
         * @return 分页的用户行为日志
         */
        virtual models::PagedActivityLogs getActivityLogs(int page, int size,
                                                          int userId = 0,
                                                          const std::string& action = "") = 0;
    };

} // namespace gocook::services
