#pragma once

#include <nlohmann/json.hpp>
#include <gocook/DataModels.h>
#include "SerializationHelper.h"

/// 所有模型 → JSON 序列化函数的唯一定义处。
/// Handler 仅需包含此头文件。

namespace JsonSerializer {

using json = nlohmann::json;
using namespace gocook::models;

// ======================== 基础模型 ========================

json toJson(const Nutrition& n);
json toJson(const Ingredient& ing);
json toJson(const CookingStep& step);
json toJson(const AvoidanceItem& item);
json toJson(const MatchIngredient& ing);
json toJson(const MissingIngredient& ing);

// ======================== 菜谱相关 ========================

json toJson(const RecipeSummary& recipe);
json toJson(const RecommendedRecipe& rec);
json toJson(const RecipeDetail& detail);
json toJson(const RecipeVideo& video);
json toJson(const RecipeRating& rating);
json toJson(const MyRecipeStatus& status);

// ======================== 营养报告 ========================

json toJson(const NutritionBreakdownItem& item);
json toJson(const NutritionReport& report);

// ======================== 用户 / 认证相关 ========================

json toJson(const UserProfile& user);
json toJson(const UserPreferences& prefs);
json toJson(const HealthProfileResponse& resp);
json toJson(const FavoriteItem& item);
json toJson(const FavoriteGroup& group);

// ======================== 通知 / 我的评论 ========================

json toJson(const NotificationItem& item);
json toJson(const UserRatingItem& item);

// ======================== 库存 / 购物清单 ========================

json toJson(const InventoryItem& item);
json toJson(const ShoppingListItem& item);
json toJson(const ShoppingListSummary& summary);
json toJson(const ShoppingList& list);
json toJson(const BatchShoppingResponse& resp);

// ======================== 膳食计划 ========================

json toJson(const MealPlanSummary& plan);
json toJson(const DailyMealDetails& daily);
json toJson(const CalendarDay& day);
json toJson(const NutritionTrendItem& trend);

// ======================== 公告 ========================

json toJson(const AnnouncementItem& ann);

// ======================== 管理员相关 ========================

json toJson(const AdminUser& user);
json toJson(const PendingRecipeItem& item);
json toJson(const BatchReviewResponse& resp);
json toJson(const NotificationResponse& resp);

// ======================== 分页包装（模板，必须在头文件） ========================

template<typename PagedType, typename Func>
inline json pagedToJson(const PagedType& paged, Func&& itemMapper) {
    json result;
    result["data"] = json::array();
    for (const auto& item : paged.data)
        result["data"].push_back(itemMapper(item));
    result["pagination"] = ::toJson(paged.pagination);
    return result;
}

inline json toJson(const PagedRecipes& paged) {
    return pagedToJson(paged, [](const RecipeSummary& r) { return toJson(r); });
}
inline json toJson(const PagedFavorites& paged) {
    return pagedToJson(paged, [](const FavoriteItem& r) { return toJson(r); });
}
inline json toJson(const PagedInventory& paged) {
    return pagedToJson(paged, [](const InventoryItem& r) { return toJson(r); });
}
inline json toJson(const PagedRatings& paged) {
    return pagedToJson(paged, [](const RecipeRating& r) { return toJson(r); });
}
inline json toJson(const PagedAnnouncements& paged) {
    return pagedToJson(paged, [](const AnnouncementItem& r) { return toJson(r); });
}
inline json toJson(const PagedMyRecipes& paged) {
    return pagedToJson(paged, [](const MyRecipeStatus& r) { return toJson(r); });
}
inline json toJson(const PagedUsers& paged) {
    return pagedToJson(paged, [](const AdminUser& u) { return toJson(u); });
}
inline json toJson(const PagedMealPlans& paged) {
    return pagedToJson(paged, [](const MealPlanSummary& r) { return toJson(r); });
}
inline json toJson(const PagedPendingRecipes& paged) {
    return pagedToJson(paged, [](const PendingRecipeItem& r) { return toJson(r); });
}
inline json toJson(const PagedNotifications& paged) {
    return pagedToJson(paged, [](const NotificationItem& r) { return toJson(r); });
}
inline json toJson(const PagedUserRatings& paged) {
    return pagedToJson(paged, [](const UserRatingItem& r) { return toJson(r); });
}
inline json toJson(const PagedCalendarDays& paged) {
    return pagedToJson(paged, [](const CalendarDay& r) { return toJson(r); });
}

inline json toJson(const PagedRecommendedRecipes& paged) {
    json j = pagedToJson(paged, [](const RecommendedRecipe& r) { return toJson(r); });
    j["health_filter_applied"] = paged.health_filter_applied;
    return j;
}

} // namespace JsonSerializer
