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
        bool valid = false;
    };

    // ==============================================
    // 用户信息
    // ==============================================

    /// 用户公开资料
    struct UserProfile {
        int id = 0;
        std::string username;
        std::string email;
        std::string phone;          // 服务端脱敏后的手机号（如 138****1234），前端直接展示
        std::string avatar_url;
        std::string created_at;
    };

    /// 更新用户个人资料请求
    struct UpdateProfileRequest {
        std::optional<std::string> username;
        std::optional<std::string> avatar_url;
        std::optional<std::string> email;
        std::optional<std::string> phone;   // 新增字段，对齐 API 3.2 节
    };

    /// 用户饮食偏好
    struct UserPreferences {
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;
        std::vector<std::string> allergies;
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

    /// 收藏菜谱项
    struct FavoriteItem {
        int id = 0;
        std::string name;
        std::string description;
        std::string image_url;
        std::string favorited_at;
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
        int prep_time_minutes = 0;
        int cook_time_minutes = 0;
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
        int prep_time_minutes = 0;
        int cook_time_minutes = 0;
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
        double suggested_quantity = 0.0;
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
    };

    // ==============================================
    // 库存与购物清单
    // ==============================================

    /// 库存项
    struct InventoryItem {
        int id = 0;                     // 新增 id 字段，对齐 API 5.1 节，用于删除等操作
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
        double required_quantity = 0.0;
        double inventory_quantity = 0.0;   // 新增字段，对齐 API 5.4 节，表示库存已有数量
        double to_buy_quantity = 0.0;
        std::string unit;
        bool checked = false;
    };

    /// 完整的购物清单响应
    struct ShoppingList {
        int id = 0;
        std::vector<ShoppingListItem> items;
    };

    /// 更新购物清单项请求
    struct UpdateShoppingItemRequest {
        bool checked = false;
    };

    /// 批量添加购物清单项请求
    struct BatchShoppingItem {
        std::string ingredient_name;
        double quantity = 0.0;
        std::string unit;
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
        std::optional<MealPlanSummary> snack;   // 可选用于零食
    };

    /// 日历视图单日信息
    struct CalendarDay {
        std::string date;
        DailyMealDetails meals;
        Nutrition daily_total;
    };

    /// 日历视图响应
    struct MealPlanCalendar {
        std::vector<CalendarDay> days;
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
    // 管理员相关
    // ==============================================

    /// 管理端用户列表项
    struct AdminUser {
        int id = 0;
        std::string username;
        std::string email;
        std::string role;           // "user", "admin"
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

    // ==============================================
    // 分页通用包装（模板）
    // ==============================================

    /// 通用分页返回结构
    template <typename T>
    struct PagedResult {
        std::vector<T> data;
        Pagination pagination;
    };

    // ---- 常见具体分页类型（方便使用，也可直接使用模板） ----
    using PagedRecipes         = PagedResult<RecipeSummary>;
    using PagedRecommendedRecipes = PagedResult<RecommendedRecipe>;
    using PagedFavorites       = PagedResult<FavoriteItem>;
    using PagedInventory       = PagedResult<InventoryItem>;
    using PagedRatings         = PagedResult<RecipeRating>;
    using PagedAnnouncements   = PagedResult<AnnouncementItem>;
    using PagedMyRecipes       = PagedResult<MyRecipeStatus>;   // 投稿列表
    using PagedUsers           = PagedResult<AdminUser>;
    using PagedMealPlans       = PagedResult<MealPlanSummary>;
    using PagedPendingRecipes  = PagedResult<PendingRecipeItem>; // 待审核菜谱列表

} // namespace gocook::models