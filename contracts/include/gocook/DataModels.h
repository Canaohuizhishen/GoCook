#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace gocook::models {

    // ==============================================
    // 通用结构
    // ==============================================

    /// 统一分页信息
    struct Pagination {
        int page = 1;
        int size = 20;
        int total = 0;
        int total_pages = 0;
    };

    /// 通用错误响应
    struct ErrorResponse {
        std::string error;
    };

    // ==============================================
    // 认证相关
    // ==============================================

    /// 注册请求
    struct RegisterRequest {
        std::string username;
        std::string password;
        std::string email;          // 必填，用于密码找回
    };

    /// 登录请求
    struct LoginRequest {
        std::string username;
        std::string password;
    };

    /// 登录成功响应
    struct LoginResponse {
        std::string token;
        int user_id = 0;
        std::string username;
    };

    /// 注册/简单操作成功消息
    struct MessageResponse {
        std::string message;
    };

    /// 从 JWT 中解析出的用户信息（用于服务端认证中间件）
    struct TokenInfo {
        int userId = 0;
        std::string username;
        std::string role;       // 用户角色：user, moderator, super_admin
        bool valid = false;
    };

    // ==============================================
    // 用户信息
    // ==============================================

    /// 用户公开资料
    struct UserProfile {
        int id = 0;
        std::string username;           // 登录凭证，不可更改
        std::string display_name;       // 显示名（昵称），可更改
        std::string email;
        std::string phone;
        std::string avatar_url;
        bool preferences_complete = false; // 核心偏好/健康信息是否已填写
        std::string created_at;
    };

    /// 更新用户个人资料请求
    struct UpdateProfileRequest {
        std::optional<std::string> display_name;
        std::optional<std::string> avatar_url;
        std::optional<int>         avatar_id;      // 头像上传返回的资源标识
        std::optional<std::string> email;          // 新增：联系邮箱
        std::optional<std::string> phone;          // 新增：联系电话
    };

    /// 用户饮食偏好（v2.8 统一禁忌字段为 dislikes）
    struct UserPreferences {
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;      // 饮食禁忌（过敏、宗教禁忌等）
        std::string health_goal;
    };

    /// 更新偏好请求（与 UserPreferences 相同，提供别名以便区分语义）
    using UpdatePreferencesRequest = UserPreferences;

    /// 健康指标录入请求
    struct HealthProfileRequest {
        std::optional<int> height_cm;
        std::optional<double> weight_kg;
        std::vector<std::string> conditions;
    };

    /// 忌口建议项
    struct AvoidanceItem {
        std::string ingredient;
        std::string reason;
    };

    /// 健康指标分析响应
    struct HealthProfileResponse {
        std::vector<AvoidanceItem> suggested_avoidances;
    };

    /// 头像上传响应
    struct AvatarUploadResponse {
        int avatar_id = 0;
        std::string avatar_url;
    };

    /// 收藏菜谱项
    struct FavoriteItem {
        int id = 0;
        std::string name;
        std::string description;
        std::string image_url;
        std::string group_name;         // 所属分组名，默认"默认收藏夹"
        bool is_public = true;          // 是否公开可见
        std::string favorited_at;
    };

    /// 收藏分组摘要
    struct FavoriteGroup {
        int id = 0;
        std::string name;
        int sort_order = 0;
        int count = 0;
    };

    /// 创建收藏分组请求
    struct CreateGroupRequest {
        std::string name;
    };

    /// 更新收藏分组请求
    struct UpdateGroupRequest {
        std::string name;
    };

    /// 更新收藏项属性请求
    struct UpdateFavoriteRequest {
        std::optional<int> group_id;
        std::optional<bool> is_public;
    };

    /// 批量删除收藏请求
    struct BatchDeleteFavoritesRequest {
        std::vector<int> favorite_ids;
    };

    // ==============================================
    // 菜谱相关
    // ==============================================

    /// 食材
    struct Ingredient {
        std::string name;
        double quantity = 0.0;
        std::string unit;
    };

    /// 烹饪步骤
    struct CookingStep {
        int order = 0;
        std::string description;
        std::optional<int> duration;   // 步骤计时（秒），对应 JSON 字段 "duration"
    };

    /// 营养信息
    struct Nutrition {
        double calories = 0.0;
        double protein = 0.0;
        double fat = 0.0;
        double carbs = 0.0;
    };

    /// 菜谱列表项（摘要）
    struct RecipeSummary {
        int id = 0;
        std::string name;
        std::string description;
        std::string image_url;
        std::string cooking_method;     // 烹饪技法（炒、炖、蒸等）
        std::string flavor;             // 口味（清淡、麻辣等）
        std::string ingredient_type;    // 食材类型（荤、素等）
        int prep_time_minutes = 0;
        int cook_time_minutes = 0;
        int calories = 0;               // 总热量
        int view_count = 0;             // 浏览量（按用户日去重）
        double avg_rating = 0.0;        // 平均评分
        std::vector<std::string> tags;
        int author_id = 0;
        std::string author_name;
    };

    /// 菜谱详情
    struct RecipeDetail {
        int id = 0;
        std::string name;
        std::string description;
        std::string image_url;
        std::string cooking_method;
        std::string flavor;
        int prep_time_minutes = 0;
        int cook_time_minutes = 0;
        int view_count = 0;
        double avg_rating = 0.0;
        std::vector<Ingredient> ingredients;
        std::vector<CookingStep> steps;
        Nutrition nutrition;
        std::vector<std::string> tags;
        int author_id = 0;
        std::string author_name;
        std::string created_at;
    };

    /// 关联视频
    struct RecipeVideo {
        int id = 0;
        std::string title;
        std::string platform;
        std::string url;
        std::string thumbnail_url;
        int duration_seconds = 0;
    };

    /// 评分与评论
    struct RecipeRating {
        int id = 0;
        int user_id = 0;
        std::string username;
        int rating = 0;
        std::string comment;
        std::string created_at;
    };

    /// 评分与评论请求体
    struct RateRecipeRequest {
        int rating;
        std::string comment;
    };

    /// 菜谱投稿请求体
    struct SubmitRecipeRequest {
        std::string name;
        std::string description;
        std::string image_url;
        std::vector<Ingredient> ingredients;
        std::vector<CookingStep> steps;
        std::optional<Nutrition> nutrition;
        std::vector<std::string> tags;
        std::optional<std::string> cooking_method;
        std::optional<std::string> flavor;
        std::optional<std::string> ingredient_type;
    };

    /// 菜谱投稿响应体（POST /api/recipes）
    struct SubmitRecipeResponse {
        int id = 0;
        std::string status;   // "pending"
    };

    /// 投稿状态（列表项）
    struct MyRecipeStatus {
        int id = 0;
        std::string name;
        std::string status;               // "pending", "approved", "rejected"
        std::optional<std::string> reject_reason;
        std::string submitted_at;
    };

    /// 菜谱编辑请求（需认证）
    using EditRecipeRequest = SubmitRecipeRequest;

    /// 智能推荐中的匹配食材信息
    struct MatchIngredient {
        std::string name;
        double quantity = 0.0;
        std::string unit;
    };

    /// 缺失食材（含建议用量）
    struct MissingIngredient {
        std::string name;
        double quantity = 0.0;   // 修正字段名，与 API 契约一致
        std::string unit;
    };

    /// 匹配详情（用于推荐菜谱）
    struct MatchStatus {
        std::vector<MatchIngredient> available_ingredients;
        std::vector<MissingIngredient> missing_ingredients;
    };

    /// 智能推荐菜谱项
    struct RecommendedRecipe : RecipeSummary {
        double match_score = 0.0;
        MatchStatus match_status;
        // ── 内部字段（服务端推荐引擎使用，不输出到客户端） ──
        double protein_g = 0.0;         // per_serving 蛋白质
        double fat_g = 0.0;             // per_serving 脂肪
        double carbs_g = 0.0;           // per_serving 碳水
        std::string submitted_at;       // 投稿时间 ISO 字符串
    };

    /// 营养报告食材明细（API 4.15）
    struct NutritionBreakdownItem {
        std::string name;
        double calories = 0.0;
        double protein_g = 0.0;   // 修正：对齐 API 字段名
        double fat_g = 0.0;
        double carbs_g = 0.0;
    };

    /// 独立营养报告（API 4.15）
    struct NutritionReport {
        int recipe_id = 0;
        std::string recipe_name;
        struct PerServing {
            double calories = 0.0;
            double protein_g = 0.0;   // 修正：对齐 API 字段名
            double fat_g = 0.0;
            double carbs_g = 0.0;
            double fiber_g = 0.0;      // 新增：膳食纤维（克）
            double sodium_mg = 0.0;    // 新增：钠（毫克）
            double vitamin_c_mg = 0.0; // 新增：维生素C（毫克）
        } per_serving;
        std::vector<NutritionBreakdownItem> ingredients_breakdown;
        std::string health_notes;
    };

    // ==============================================
    // 库存与购物清单
    // ==============================================

    /// 库存项
    struct InventoryItem {
        int id = 0;
        std::string ingredient_name;
        double quantity = 0.0;
        std::string unit;
        std::optional<std::string> expiry_date;
        std::string added_at;
    };

    /// 添加/更新库存请求
    struct UpsertInventoryRequest {
        std::string ingredient_name;
        double quantity = 0.0;
        std::string unit;
        std::optional<std::string> expiry_date;
    };

    /// 购物清单项
    struct ShoppingListItem {
        int id = 0;
        std::string ingredient_name;
        double required_quantity = 0.0;  // 新增：膳食计划/菜谱总共需要的食材数量
        double inventory_quantity = 0.0;
        double to_buy_quantity = 0.0;
        std::string unit;
        bool checked = false;
    };

    /// 完整的购物清单详情
    struct ShoppingList {
        int id = 0;
        std::string name;
        std::vector<ShoppingListItem> items;
    };

    /// 购物清单摘要（列表项）
    struct ShoppingListSummary {
        int id = 0;
        std::string name;
        int item_count = 0;
        std::string created_at;
    };

    /// 创建购物清单请求
    struct CreateShoppingListRequest {
        std::string name;
        std::optional<std::string> plan_id;     // 修正：API 可能传递字符串类型的 plan_id
    };

    /// 更新购物清单项请求
    struct UpdateShoppingItemRequest {
        bool checked = false;
    };

    /// 批量添加购物清单项请求
    struct BatchShoppingItem {
        std::string ingredient_name;
        double quantity = 0.0;
        std::string unit;    // 新增：单位
    };

    /// 批量添加购物清单项响应
    struct BatchShoppingResponse {
        std::string message;
        std::vector<ShoppingListItem> items;
    };

    // ==============================================
    // 膳食计划
    // ==============================================

    /// 创建/更新膳食计划请求
    struct MealPlanRequest {
        int recipe_id = 0;
        std::string date;          // "YYYY-MM-DD"
        std::string meal_type;     // "breakfast", "lunch", "dinner", "snack"
    };

    /// 膳食计划概要
    struct MealPlanSummary {
        int plan_id = 0;
        std::string date;
        std::string meal_type;
        RecipeSummary recipe;       // 菜谱简略信息
        Nutrition nutrition;
    };

    /// 日历视图某一天的膳食
    struct DailyMealDetails {
        std::optional<MealPlanSummary> breakfast;
        std::optional<MealPlanSummary> lunch;
        std::optional<MealPlanSummary> dinner;
        std::optional<MealPlanSummary> snack;   // 新增：零食/加餐
    };

    /// 日历视图单日信息
    struct CalendarDay {
        std::string date;
        DailyMealDetails meals;
        Nutrition daily_total;
    };

    /// 营养趋势: 对比项
    struct NutritionComparisonItem {
        double diff = 0.0;
        double percentage = 0.0;
    };

    /// 每日营养对比
    struct DailyNutritionComparison {
        NutritionComparisonItem calories;
        NutritionComparisonItem protein;
        NutritionComparisonItem fat;
        NutritionComparisonItem carbs;
    };

    /// 营养趋势每日数据
    struct NutritionTrendItem {
        std::string date;
        Nutrition actual;
        Nutrition recommended;
        DailyNutritionComparison comparison;
    };

    /// 营养趋势响应
    struct NutritionTrendResponse {
        std::vector<NutritionTrendItem> trend;
        Nutrition daily_goals;
    };

    /// 膳食计划分页响应（包含营养汇总）
    struct MealPlansResponse {
        std::vector<MealPlanSummary> data;
        Pagination pagination;
        struct NutritionSummary {
            double total_calories = 0.0;
            double avg_protein = 0.0;
        };
        std::optional<NutritionSummary> nutrition_summary;
    };

    // ==============================================
    // 通知中心（API 3.12）
    // ==============================================

    /// 通知项
    struct NotificationItem {
        int id = 0;
        std::string title;
        std::string content;
        std::string type;               // "system", "review", "interaction"
        std::string sub_type;           // 用于 interaction 的细分
        bool is_read = false;
        int related_id = 0;
        std::string trigger_user_name;
        std::string created_at;
    };

    // ==============================================
    // 我的评论项（API 3.14）
    // ==============================================

    /// 用户自己的评论摘要
    struct UserRatingItem {
        int rating_id = 0;
        int recipe_id = 0;
        std::string recipe_name;
        int rating = 0;
        std::string comment;
        std::string created_at;
        std::string updated_at;
    };

    // ==============================================
    // 管理员相关
    // ==============================================

    /// 管理端用户列表项
    struct AdminUser {
        int id = 0;
        std::string username;
        std::string email;
        std::string role;           // "user", "moderator", "super_admin"
        std::string status;         // "active", "frozen"
        std::string created_at;
    };

    /// 创建用户（管理员）
    struct CreateUserRequest {
        std::string username;
        std::string password;
        std::string email;
        std::string role;
    };

    /// 修改用户（管理员）
    struct UpdateUserRequest {
        std::optional<std::string> email;
        std::optional<std::string> role;
    };

    /// 设置用户状态
    struct SetUserStatusRequest {
        std::string status;         // "active" or "frozen"
    };

    /// 待审核菜谱列表项（管理员）
    struct PendingRecipeItem {
        int id = 0;
        std::string name;
        int author_id = 0;
        std::string author_name;
        std::string submitted_at;
        std::string status;         // "pending"
    };

    /// 审核拒绝菜谱请求
    struct RejectRecipeRequest {
        std::string reason;
    };

    /// 批量审核请求
    struct BatchReviewRequest {
        std::vector<int> recipe_ids;
        std::string action;         // "approve" or "reject"
        std::optional<std::string> reason;
    };

    /// 批量审核响应
    struct BatchReviewResponse {
        std::string message;
        int success_count = 0;
        std::vector<int> failed_ids;
    };

    /// 发布公告请求
    struct AnnouncementRequest {
        std::string title;
        std::string content;
    };

    /// 公告项
    struct AnnouncementItem {
        int id = 0;
        std::string title;
        std::string content;
        std::string created_at;
    };

    /// 发送通知请求
    struct NotificationRequest {
        std::string title;
        std::string content;
        std::string target_type;    // "all" or "specific"
        std::vector<int> target_ids;
        std::vector<std::string> channels; // "in_app", "email"
        std::optional<std::string> scheduled_at;  // ISO 8601 时间，null表示立即发送
    };

    /// 通知发送响应
    struct NotificationResponse {
        int notification_id = 0;
        std::string status;
        int estimated_recipients = 0;
    };

    /// 库存分类（用于统计分布）
    struct InventoryCategory {
        std::string name;
        int count = 0;
    };

    /// 运营数据统计（API 7.8）
    struct StatisticsData {
        int total_users = 0;
        int active_users_7d = 0;
        int total_recipes = 0;
        int pending_reviews = 0;
        int new_recipes_week = 0;
        int new_comments_week = 0;
        struct Growth {
            int new_users_week = 0;
            double new_users_week_growth = 0.0;
            double new_recipes_week_growth = 0.0;
            double active_users_7d_growth = 0.0;
        } growth;
        struct TopRecipe {
            int id = 0;
            std::string name;
            int view_count = 0;
        };
        std::vector<TopRecipe> top_recipes;
        struct InventoryDistribution {
            std::vector<InventoryCategory> categories; // 对齐 API：包含 categories 数组的对象
        } inventory_distribution;
    };

    /// 管理员操作日志项（API 7.9）
    struct AdminLogItem {
        int id = 0;
        int operator_id = 0;
        std::string operator_name;
        std::string type;
        std::string action;
        int target_id = 0;
        std::string target_name;
        std::string detail;
        std::string result;
        std::string created_at;
    };

    /// 用户行为日志项（API 7.10）
    struct UserActivityLogItem {
        int id = 0;
        int user_id = 0;
        std::string username;
        std::string action;
        std::string target_type;
        int target_id = 0;
        std::string detail;
        std::string created_at;
    };

    // ==============================================
    // 分页通用包装（模板）
    // ==============================================

    /// 通用分页返回结构
    template <typename T>
    struct PagedResult {
        std::vector<T> data;
        Pagination pagination;
    };

    // ---- 常见具体分页类型 ----
    using PagedRecipes         = PagedResult<RecipeSummary>;
    using PagedFavorites       = PagedResult<FavoriteItem>;
    using PagedInventory       = PagedResult<InventoryItem>;
    using PagedRatings         = PagedResult<RecipeRating>;
    using PagedAnnouncements   = PagedResult<AnnouncementItem>;
    using PagedMyRecipes       = PagedResult<MyRecipeStatus>;
    using PagedUsers           = PagedResult<AdminUser>;
    using PagedMealPlans       = PagedResult<MealPlanSummary>;
    using PagedCalendarDays    = PagedResult<CalendarDay>;
    using PagedPendingRecipes  = PagedResult<PendingRecipeItem>;
    using PagedNotifications   = PagedResult<NotificationItem>;
    using PagedUserRatings     = PagedResult<UserRatingItem>;
    using PagedAdminLogs       = PagedResult<AdminLogItem>;
    using PagedActivityLogs    = PagedResult<UserActivityLogItem>;

    /// 智能推荐分页响应，包含健康过滤标记（API 4.2）
    struct PagedRecommendedRecipes : public PagedResult<RecommendedRecipe> {
        bool health_filter_applied = false;
    };

} // namespace gocook::models