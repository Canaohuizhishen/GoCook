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
        int page = 1;            // 页码（从 1 开始）
        int size = 20;           // 每页数量
        int total = 0;           // 总记录数
        int total_pages = 0;     // 总页数
    };

    /// 通用错误响应
    struct ErrorResponse {
        std::string error;   // 错误描述（中文）
    };

    // ==============================================
    // 认证相关
    // ==============================================

    /// 注册请求
    struct RegisterRequest {
        std::string username;      // 用户名
        std::string password;      // 密码（明文，服务端哈希后存储）
        std::string email;         // 必填，用于密码找回
    };

    /// 登录请求
    struct LoginRequest {
        std::string username;   // 用户名
        std::string password;   // 密码（明文）
    };

    /// 登录成功响应
    struct LoginResponse {
        std::string token;       // JWT 访问令牌
        int user_id = 0;         // 用户 ID
        std::string username;    // 用户名
    };

    /// 注册/简单操作成功消息
    struct MessageResponse {
        std::string message;   // 成功提示消息
    };

    /// 从 JWT 中解析出的用户信息（用于服务端认证中间件）
    struct TokenInfo {
        int userId = 0;          // 用户 ID
        std::string username;    // 用户名
        std::string role;        // 用户角色：user, moderator, super_admin
        bool valid = false;      // 令牌是否验证通过
    };

    // ==============================================
    // 用户信息
    // ==============================================

    /// 用户公开资料
    struct UserProfile {
        int id = 0;                          // 用户 ID
        std::string username;                // 登录凭证，不可更改
        std::string display_name;            // 显示名（昵称），可更改
        std::string email;                   // 联系邮箱
        std::string phone;                   // 联系电话
        std::string avatar_url;              // 头像 URL（相对路径）
        bool preferences_complete = false;   // 核心偏好/健康信息是否已填写
        std::string created_at;              // 注册时间（ISO 8601）
    };

    /// 更新用户个人资料请求
    struct UpdateProfileRequest {
        std::optional<std::string> display_name;   // 显示名（昵称），nullopt 表示不修改
        std::optional<std::string> avatar_url;     // 头像 URL
        std::optional<int>         avatar_id;      // 头像上传返回的资源标识
        std::optional<std::string> email;          // 新增：联系邮箱
        std::optional<std::string> phone;          // 新增：联系电话
    };

    /// 用户饮食偏好（v2.8 统一禁忌字段为 dislikes）
    struct UserPreferences {
        std::vector<std::string> likes;         // 喜爱的食材/口味
        std::vector<std::string> dislikes;      // 饮食禁忌（过敏、宗教禁忌等）
        std::string health_goal;                // 健康目标（如减脂、增肌）
    };

    /// 更新偏好请求（与 UserPreferences 相同，提供别名以便区分语义）
    using UpdatePreferencesRequest = UserPreferences;

    /// 健康指标录入请求
    struct HealthProfileRequest {
        std::optional<int> height_cm;              // 身高（厘米），nullopt 表示未设置
        std::optional<double> weight_kg;           // 体重（千克），nullopt 表示未设置
        std::vector<std::string> conditions;       // 健康条件（如高血压、糖尿病）
    };

    /// 忌口建议项
    struct AvoidanceItem {
        std::string ingredient;   // 需忌口的食材
        std::string reason;       // 忌口原因（如过敏、健康条件）
    };

    /// 健康指标分析响应（也作为 GET 响应体，包含全部字段）
    struct HealthProfileResponse {
        std::optional<int> height_cm;                    // 身高（厘米），nullopt 表示未设置
        std::optional<double> weight_kg;                 // 体重（千克），nullopt 表示未设置
        std::vector<std::string> conditions;             // 健康条件列表
        std::vector<AvoidanceItem> suggested_avoidances; // 建议忌口项列表
    };

    /// 头像上传响应
    struct AvatarUploadResponse {
        int avatar_id = 0;      // 头像资源 ID
        std::string avatar_url; // 头像 URL（相对路径）
    };

    /// 收藏菜谱项
    struct FavoriteItem {
        int id = 0;                     // 收藏记录 ID
        int recipe_id = 0;              // 菜谱 ID（用于导航到详情页）
        std::string name;               // 菜谱名称
        std::string description;        // 菜谱简介
        std::string image_url;          // 菜谱封面图 URL
        std::string group_name;         // 所属分组名，默认"默认收藏夹"
        bool is_public = true;          // 是否公开可见
        std::string favorited_at;       // 收藏时间（ISO 8601）
    };

    /// 收藏分组摘要
    struct FavoriteGroup {
        int id = 0;              // 分组 ID
        std::string name;        // 分组名
        int sort_order = 0;      // 排序序号（升序）
        int count = 0;           // 分组内收藏数量
    };

    /// 创建收藏分组请求
    struct CreateGroupRequest {
        std::string name;   // 分组名
    };

    /// 更新收藏分组请求
    struct UpdateGroupRequest {
        std::string name;   // 新的分组名
    };

    /// 更新收藏项属性请求
    struct UpdateFavoriteRequest {
        std::optional<int> group_id;     // 目标分组 ID，nullopt 表示不修改
        std::optional<bool> is_public;   // 是否公开可见，nullopt 表示不修改
    };

    /// 批量删除收藏请求
    struct BatchDeleteFavoritesRequest {
        std::vector<int> favorite_ids;   // 待删除的收藏记录 ID 列表
    };

    // ==============================================
    // 菜谱相关
    // ==============================================

    /// 食材
    struct Ingredient {
        std::string name;        // 食材名称
        double quantity = 0.0;   // 用量数值
        std::string unit;        // 用量单位（如克、个）
    };

    /// 烹饪步骤
    struct CookingStep {
        int order = 0;               // 步骤序号（从 1 开始）
        std::string description;     // 步骤说明
        std::optional<int> duration; // 步骤计时（秒），对应 JSON 字段 "duration"
        std::string image_url;       // 步骤图 URL（相对路径）
    };

    /// 营养信息
    struct Nutrition {
        double calories = 0.0;   // 热量（千卡）
        double protein = 0.0;    // 蛋白质（克）
        double fat = 0.0;        // 脂肪（克）
        double carbs = 0.0;      // 碳水化合物（克）
        // 是否有可用营养数据。默认 false（安全方向：缺字段时前端展示空态而非全 0 假数据）
        bool has_data = false;
    };

    /// 菜谱列表项（摘要）
    struct RecipeSummary {
        int id = 0;                     // 菜谱 ID
        std::string name;               // 菜谱名称
        std::string description;        // 菜谱简介
        std::string image_url;          // 封面图 URL
        std::string cooking_method;     // 烹饪技法（炒、炖、蒸等）
        std::string flavor;             // 口味（清淡、麻辣等）
        std::string ingredient_type;    // 食材类型（荤、素等）
        int prep_time_minutes = 0;      // 准备时间（分钟）
        int cook_time_minutes = 0;      // 烹饪时间（分钟）
        int calories = 0;               // 总热量
        int view_count = 0;             // 浏览量（按用户日去重）
        double avg_rating = 0.0;        // 平均评分
        std::vector<std::string> tags;  // 标签列表
        int author_id = 0;              // 作者用户 ID
        std::string author_name;        // 作者用户名
    };

    /// 菜谱详情
    struct RecipeDetail {
        int id = 0;                              // 菜谱 ID
        std::string name;                        // 菜谱名称
        std::string description;                 // 菜谱简介
        std::string image_url;                   // 封面图 URL
        std::string cooking_method;              // 烹饪技法（炒、炖、蒸等）
        std::string flavor;                      // 口味（清淡、麻辣等）
        int prep_time_minutes = 0;               // 准备时间（分钟）
        int cook_time_minutes = 0;               // 烹饪时间（分钟）
        int view_count = 0;                      // 浏览量（按用户日去重）
        double avg_rating = 0.0;                 // 平均评分
        bool is_favorited = false;               // 当前用户是否已收藏
        std::vector<Ingredient> ingredients;     // 食材列表
        std::vector<CookingStep> steps;          // 烹饪步骤列表
        Nutrition nutrition;                     // 营养信息
        std::vector<std::string> tags;           // 标签列表
        int author_id = 0;                       // 作者用户 ID
        std::string author_name;                 // 作者用户名
        std::string created_at;                  // 创建时间（ISO 8601）
        std::string updated_at;                  // 更新时间（ISO 8601）
    };

    /// 关联视频
    struct RecipeVideo {
        int id = 0;                     // 视频 ID
        std::string title;              // 视频标题
        std::string platform;           // 视频平台（如 bilibili、抖音）
        std::string url;                // 视频链接
        std::string thumbnail_url;      // 视频封面图 URL
        int duration_seconds = 0;       // 视频时长（秒）
    };

    /// 评分与评论
    struct RecipeRating {
        int id = 0;                 // 评论 ID
        int user_id = 0;            // 评论用户 ID
        std::string username;       // 评论用户名
        int rating = 0;             // 评分（1-5）
        std::string comment;        // 评论内容
        std::string created_at;     // 评论时间（ISO 8601）
    };

    /// 评分与评论请求体
    struct RateRecipeRequest {
        int rating;           // 评分（1-5）
        std::string comment;  // 评论内容（可为空）
    };

    /// 菜谱投稿请求体
    struct SubmitRecipeRequest {
        std::string name;                           // 菜谱名称
        std::string description;                    // 菜谱简介
        std::string image_url;                      // 封面图 URL
        std::vector<Ingredient> ingredients;        // 食材列表
        std::vector<CookingStep> steps;             // 烹饪步骤列表
        std::optional<Nutrition> nutrition;         // 营养信息（可选）
        std::vector<std::string> tags;              // 标签列表
        std::optional<std::string> cooking_method;  // 烹饪技法（可选）
        std::optional<std::string> flavor;          // 口味（可选）
        std::optional<std::string> ingredient_type; // 食材类型（可选）
    };

    /// 菜谱投稿响应体（POST /api/recipes）
    struct SubmitRecipeResponse {
        int id = 0;              // 新菜谱 ID
        std::string status;      // "pending"
    };

    /// 投稿状态（列表项）
    struct MyRecipeStatus {
        int id = 0;                               // 菜谱 ID
        std::string name;                         // 菜谱名称
        std::string status;                       // "pending", "approved", "rejected"
        std::optional<std::string> reject_reason; // 审核拒绝原因（未拒绝时为 nullopt）
        std::string submitted_at;                 // 投稿时间（ISO 8601）
        std::string updated_at;                   // 最后更新时间（ISO 8601）
    };

    /// 菜谱编辑请求（需认证）
    using EditRecipeRequest = SubmitRecipeRequest;

    /// 智能推荐中的匹配食材信息
    struct MatchIngredient {
        std::string name;        // 食材名称
        double quantity = 0.0;   // 用量数值
        std::string unit;        // 用量单位
    };

    /// 缺失食材（含建议用量）
    struct MissingIngredient {
        std::string name;         // 食材名称
        double quantity = 0.0;    // 修正字段名，与 API 契约一致
        std::string unit;         // 用量单位
    };

    /// 匹配详情（用于推荐菜谱）
    struct MatchStatus {
        std::vector<MatchIngredient> available_ingredients;  // 可用食材列表
        std::vector<MissingIngredient> missing_ingredients;  // 缺失食材列表
    };

    /// 智能推荐菜谱项
    struct RecommendedRecipe : RecipeSummary {
        double match_score = 0.0;   // 匹配度得分
        MatchStatus match_status;   // 与用户库存的匹配详情
        // ── 内部字段（服务端推荐引擎使用，不输出到客户端） ──
        double protein_g = 0.0;         // per_serving 蛋白质
        double fat_g = 0.0;             // per_serving 脂肪
        double carbs_g = 0.0;           // per_serving 碳水
        std::string submitted_at;       // 投稿时间 ISO 字符串
    };

    /// 营养报告食材明细（API 4.15）
    struct NutritionBreakdownItem {
        std::string name;          // 食材名称
        double calories = 0.0;     // 热量（千卡）
        double protein_g = 0.0;    // 修正：对齐 API 字段名
        double fat_g = 0.0;        // 脂肪（克）
        double carbs_g = 0.0;      // 碳水化合物（克）
    };

    /// 未计入营养计算的食材（营养库未收录或用量无法换算）
    struct ExcludedIngredient {
        std::string name;          // 食材名（投稿时的原始输入）
        std::string reason;        // 原因："未收录" | "无法换算"
    };

    /// 独立营养报告（API 4.15）
    struct NutritionReport {
        int recipe_id = 0;                                   // 菜谱 ID
        std::string recipe_name;                             // 菜谱名称
        /// 营养含量（当前版本语义：整道菜合计，非单份；后续可由 servings 除法升级）
        struct PerServing {
            double calories = 0.0;      // 热量（千卡）
            double protein_g = 0.0;     // 修正：对齐 API 字段名
            double fat_g = 0.0;         // 脂肪（克）
            double carbs_g = 0.0;       // 碳水化合物（克）
            double fiber_g = 0.0;       // 新增：膳食纤维（克）
            double sodium_mg = 0.0;     // 新增：钠（毫克）
            double vitamin_c_mg = 0.0;  // 新增：维生素C（毫克）
        } per_serving;                                       // 每份营养含量
        std::vector<NutritionBreakdownItem> ingredients_breakdown; // 食材营养明细列表
        std::vector<ExcludedIngredient> excluded_ingredients;      // 未计入营养的食材（可能为空）
        std::string health_notes;                            // 健康提示（文本）
        bool has_data = false;                               // 是否有可用营养数据（false=该菜谱暂无营养报告）
    };

    // ==============================================
    // 库存与购物清单
    // ==============================================

    /// 库存项
    struct InventoryItem {
        int id = 0;                             // 库存项 ID
        std::string ingredient_name;            // 食材名称
        double quantity = 0.0;                  // 库存数量
        std::string unit;                       // 数量单位
        std::optional<std::string> expiry_date; // 保质期（YYYY-MM-DD），nullopt 表示无
        std::string added_at;                   // 入库时间（ISO 8601）
    };

    /// 添加/更新库存请求
    struct UpsertInventoryRequest {
        std::string ingredient_name;            // 食材名称
        double quantity = 0.0;                  // 库存数量
        std::string unit;                       // 数量单位
        std::optional<std::string> expiry_date; // 保质期（YYYY-MM-DD），nullopt 表示无
    };

    /// 购物清单项
    struct ShoppingListItem {
        int id = 0;                        // 清单项 ID
        std::string ingredient_name;       // 食材名称
        double required_quantity = 0.0;    // 新增：膳食计划/菜谱总共需要的食材数量
        double inventory_quantity = 0.0;   // 库存中已有的数量
        double to_buy_quantity = 0.0;      // 需要购买的数量（需购量）
        std::string unit;                  // 数量单位
        bool checked = false;              // 是否已购买/勾选
    };

    /// 完整的购物清单详情
    struct ShoppingList {
        int id = 0;                          // 清单 ID
        std::string name;                    // 清单名称
        std::vector<ShoppingListItem> items; // 清单条目列表
    };

    /// 购物清单摘要（列表项）
    struct ShoppingListSummary {
        int id = 0;               // 清单 ID
        std::string name;         // 清单名称
        int item_count = 0;       // 条目数量
        std::string created_at;   // 创建时间（ISO 8601）
    };

    /// 创建购物清单请求
    struct CreateShoppingListRequest {
        std::string name;                              // 清单名称
        std::optional<std::string> plan_id;            // 修正：API 可能传递字符串类型的 plan_id
    };

    /// 更新购物清单项请求
    struct UpdateShoppingItemRequest {
        bool checked = false;   // 是否已购买/勾选
    };

    /// 批量添加购物清单项请求
    struct BatchShoppingItem {
        std::string ingredient_name;   // 食材名称
        double quantity = 0.0;         // 数量
        std::string unit;              // 新增：单位
    };

    /// 批量添加购物清单项响应
    struct BatchShoppingResponse {
        std::string message;                     // 处理结果消息
        std::vector<ShoppingListItem> items;     // 完整的清单条目列表
    };

    // ==============================================
    // 膳食计划
    // ==============================================

    /// 创建/更新膳食计划请求
    struct MealPlanRequest {
        int recipe_id = 0;       // 菜谱 ID
        std::string date;        // "YYYY-MM-DD"
        std::string meal_type;   // "breakfast", "lunch", "dinner", "snack"
    };

    /// 膳食计划概要
    struct MealPlanSummary {
        int plan_id = 0;             // 膳食计划项 ID
        std::string date;            // 计划日期（YYYY-MM-DD）
        std::string meal_type;       // 餐次：breakfast/lunch/dinner/snack
        RecipeSummary recipe;        // 菜谱简略信息
        Nutrition nutrition;         // 该餐营养信息
    };

    /// 日历视图某一天的膳食
    struct DailyMealDetails {
        std::optional<MealPlanSummary> breakfast;   // 早餐（未安排时为 nullopt）
        std::optional<MealPlanSummary> lunch;       // 午餐（未安排时为 nullopt）
        std::optional<MealPlanSummary> dinner;      // 晚餐（未安排时为 nullopt）
        std::optional<MealPlanSummary> snack;       // 新增：零食/加餐
    };

    /// 日历视图单日信息
    struct CalendarDay {
        std::string date;              // 日期（YYYY-MM-DD）
        DailyMealDetails meals;        // 当日三餐详情
        Nutrition daily_total;         // 当日营养合计
    };

    /// 营养趋势: 对比项
    struct NutritionComparisonItem {
        double diff = 0.0;          // 与推荐值的差值
        double percentage = 0.0;    // 与推荐值的百分比
    };

    /// 每日营养对比
    struct DailyNutritionComparison {
        NutritionComparisonItem calories;   // 热量对比
        NutritionComparisonItem protein;    // 蛋白质对比
        NutritionComparisonItem fat;        // 脂肪对比
        NutritionComparisonItem carbs;      // 碳水化合物对比
    };

    /// 营养趋势每日数据
    struct NutritionTrendItem {
        std::string date;                     // 日期（YYYY-MM-DD）
        Nutrition actual;                     // 当日实际摄入
        Nutrition recommended;                // 当日推荐摄入
        DailyNutritionComparison comparison;  // 实际与推荐的对比
    };

    /// 营养趋势响应
    struct NutritionTrendResponse {
        std::vector<NutritionTrendItem> trend;   // 趋势数据列表（按日期）
        Nutrition daily_goals;                   // 每日营养目标
    };

    /// 膳食计划分页响应（包含营养汇总）
    struct MealPlansResponse {
        std::vector<MealPlanSummary> data;            // 膳食计划列表
        Pagination pagination;                        // 分页信息
        /// 营养汇总
        struct NutritionSummary {
            double total_calories = 0.0;    // 总热量（千卡）
            double avg_protein = 0.0;       // 平均蛋白质（克）
        };
        std::optional<NutritionSummary> nutrition_summary;  // 营养汇总（可选）
    };

    // ==============================================
    // 通知中心（API 3.12）
    // ==============================================

    /// 通知项
    struct NotificationItem {
        int id = 0;                                        // 通知 ID
        std::string title;                                 // 通知标题
        std::string content;                               // 通知内容
        std::string type;                                  // "system", "review", "interaction"
        std::string sub_type;                              // 用于 interaction 的细分
        bool is_read = false;                              // 是否已读
        std::optional<int> related_id;                     // 关联资源 ID（如菜谱/评论），nullopt 表示无
        std::optional<std::string> trigger_user_name;      // 触发用户用户名（互动类通知）
        std::string created_at;                            // 创建时间（ISO 8601）
    };

    // ==============================================
    // 我的评论项（API 3.14）
    // ==============================================

    /// 用户自己的评论摘要
    struct UserRatingItem {
        int rating_id = 0;           // 评论 ID
        int recipe_id = 0;           // 菜谱 ID
        std::string recipe_name;     // 菜谱名称
        int rating = 0;              // 评分（1-5）
        std::string comment;         // 评论内容
        std::string created_at;      // 评论时间（ISO 8601）
        std::string updated_at;      // 更新时间（ISO 8601）
    };

    // ==============================================
    // 管理员相关
    // ==============================================

    /// 管理端用户列表项
    struct AdminUser {
        int id = 0;                 // 用户 ID
        std::string username;       // 用户名
        std::string email;          // 邮箱
        std::string role;           // "user", "moderator", "super_admin"
        std::string status;         // "active", "frozen"
        std::string created_at;     // 注册时间（ISO 8601）
    };

    /// 创建用户（管理员）
    struct CreateUserRequest {
        std::string username;    // 用户名
        std::string password;    // 初始密码（明文）
        std::string email;       // 邮箱
        std::string role;        // 角色："user", "moderator", "super_admin"
    };

    /// 修改用户（管理员）
    struct UpdateUserRequest {
        std::optional<std::string> email;   // 新邮箱，nullopt 表示不修改
        std::optional<std::string> role;    // 新角色，nullopt 表示不修改
    };

    /// 设置用户状态
    struct SetUserStatusRequest {
        std::string status;         // "active" or "frozen"
    };

    /// 待审核菜谱列表项（管理员）
    struct PendingRecipeItem {
        int id = 0;                 // 菜谱 ID
        std::string name;           // 菜谱名称
        int author_id = 0;          // 作者用户 ID
        std::string author_name;    // 作者用户名
        std::string submitted_at;   // 投稿时间（ISO 8601）
        std::string status;         // "pending"
    };

    /// 审核拒绝菜谱请求
    struct RejectRecipeRequest {
        std::string reason;   // 拒绝原因
    };

    /// 批量审核请求
    struct BatchReviewRequest {
        std::vector<int> recipe_ids;           // 待审核的菜谱 ID 列表
        std::string action;                    // "approve" or "reject"
        std::optional<std::string> reason;     // 拒绝原因（action 为 reject 时必填）
    };

    /// 批量审核响应
    struct BatchReviewResponse {
        std::string message;            // 处理结果消息
        int success_count = 0;          // 成功审核的数量
        std::vector<int> failed_ids;    // 审核失败的菜谱 ID 列表
    };

    /// 发布公告请求
    struct AnnouncementRequest {
        std::string title;      // 公告标题
        std::string content;    // 公告内容
    };

    /// 公告项
    struct AnnouncementItem {
        int id = 0;               // 公告 ID
        std::string title;        // 公告标题
        std::string content;      // 公告内容
        std::string created_at;   // 发布时间（ISO 8601）
    };

    /// 发送通知请求
    struct NotificationRequest {
        std::string title;                     // 通知标题
        std::string content;                   // 通知内容
        std::string target_type;               // "all" or "specific"
        std::vector<int> target_ids;           // 目标用户 ID 列表（target_type 为 specific 时使用）
        std::vector<std::string> channels;     // "in_app", "email"
        std::optional<std::string> scheduled_at;  // ISO 8601 时间，null表示立即发送
    };

    /// 通知发送响应
    struct NotificationResponse {
        int notification_id = 0;        // 通知 ID
        std::string status;             // 发送状态（如 "scheduled"/"sent"）
        int estimated_recipients = 0;   // 预计接收人数
    };

    /// 库存分类（用于统计分布）
    struct InventoryCategory {
        std::string name;    // 分类名称
        int count = 0;       // 该分类的库存条目数
    };

    /// 运营数据统计（API 7.8）
    struct StatisticsData {
        int total_users = 0;                  // 用户总数
        int active_users_7d = 0;              // 近 7 天活跃用户数
        int total_recipes = 0;                // 菜谱总数
        int pending_reviews = 0;              // 待审核菜谱数
        int new_recipes_week = 0;             // 本周新增菜谱数
        int new_comments_week = 0;            // 本周新增评论数
        /// 增长指标
        struct Growth {
            int new_users_week = 0;               // 本周新增用户数
            double new_users_week_growth = 0.0;   // 新增用户环比增长率
            double new_recipes_week_growth = 0.0; // 新增菜谱环比增长率
            double active_users_7d_growth = 0.0;  // 活跃用户环比增长率
        } growth;                                 // 增长指标
        /// 热门菜谱
        struct TopRecipe {
            int id = 0;               // 菜谱 ID
            std::string name;         // 菜谱名称
            int view_count = 0;       // 浏览量
        };
        std::vector<TopRecipe> top_recipes;       // 热门菜谱列表
        /// 库存分布
        struct InventoryDistribution {
            std::vector<InventoryCategory> categories; // 对齐 API：包含 categories 数组的对象
        } inventory_distribution;                 // 库存分布统计
    };

    /// 管理员操作日志项（API 7.9）
    struct AdminLogItem {
        int id = 0;                    // 日志 ID
        int operator_id = 0;           // 操作者用户 ID
        std::string operator_name;     // 操作者用户名
        std::string type;              // 日志类型（如用户管理、菜谱审核）
        std::string action;            // 操作动作描述
        int target_id = 0;             // 目标资源 ID
        std::string target_name;       // 目标资源名称
        std::string detail;            // 操作详情
        std::string result;            // 操作结果（如 success/failed）
        std::string created_at;        // 操作时间（ISO 8601）
    };

    /// 用户行为日志项（API 7.10）
    struct UserActivityLogItem {
        int id = 0;                     // 日志 ID
        int user_id = 0;                // 行为用户 ID
        std::string username;           // 行为用户名
        std::string action;             // 行为动作（如浏览、收藏、投稿）
        std::string target_type;        // 目标资源类型（如 recipe、comment）
        int target_id = 0;              // 目标资源 ID
        std::string detail;             // 行为详情
        std::string created_at;         // 行为时间（ISO 8601）
    };

    // ==============================================
    // 分页通用包装（模板）
    // ==============================================

    /// 通用分页返回结构
    template <typename T>
    struct PagedResult {
        std::vector<T> data;        // 当前页数据列表
        Pagination pagination;      // 分页信息
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
        bool health_filter_applied = false;   // 本次推荐是否应用了健康过滤
    };

} // namespace gocook::models
