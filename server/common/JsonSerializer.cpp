#include "JsonSerializer.h"

namespace JsonSerializer {

using json = nlohmann::json;
using namespace gocook::models;

// ======================== 基础模型 ========================

json toJson(const Nutrition& n) {
    return {{"calories", n.calories}, {"protein", n.protein}, {"fat", n.fat}, {"carbs", n.carbs},
            {"has_data", n.has_data}};
}

json toJson(const Ingredient& ing) {
    return {{"name", ing.name}, {"quantity", ing.quantity}, {"unit", ing.unit}};
}

json toJson(const CookingStep& step) {
    json j = {{"order", step.order}, {"description", step.description}};
    if (step.duration.has_value()) j["duration"] = step.duration.value();
    if (!step.image_url.empty()) j["image_url"] = step.image_url;
    return j;
}

json toJson(const AvoidanceItem& item) {
    return {{"ingredient", item.ingredient}, {"reason", item.reason}};
}

json toJson(const MatchIngredient& ing) {
    return {{"name", ing.name}, {"quantity", ing.quantity}, {"unit", ing.unit}};
}

json toJson(const MissingIngredient& ing) {
    return {{"name", ing.name}, {"quantity", ing.quantity}, {"unit", ing.unit}};
}

// ======================== 菜谱相关 ========================

json toJson(const RecipeSummary& recipe) {
    return {
        {"id", recipe.id},
        {"name", recipe.name},
        {"description", recipe.description},
        {"image_url", recipe.image_url},
        {"cooking_method", recipe.cooking_method},
        {"flavor", recipe.flavor},
        {"ingredient_type", recipe.ingredient_type},
        {"prep_time_minutes", recipe.prep_time_minutes},
        {"cook_time_minutes", recipe.cook_time_minutes},
        {"calories", recipe.calories},
        {"view_count", recipe.view_count},
        {"avg_rating", recipe.avg_rating},
        {"tags", recipe.tags},
        {"author_id", recipe.author_id},
        {"author_name", recipe.author_name}
    };
}

json toJson(const RecommendedRecipe& rec) {
    json item = toJson(static_cast<const RecipeSummary&>(rec));
    item["match_score"] = rec.match_score;
    item["health_notice"] = rec.health_notice;  // 健康软提示（空串 = 无提示）
    json matchStatus;
    json available = json::array();
    for (const auto& ing : rec.match_status.available_ingredients)
        available.push_back(toJson(ing));
    matchStatus["available_ingredients"] = available;
    json missing = json::array();
    for (const auto& ing : rec.match_status.missing_ingredients)
        missing.push_back(toJson(ing));
    matchStatus["missing_ingredients"] = missing;
    item["match_status"] = matchStatus;
    return item;
}

json toJson(const RecipeDetail& detail) {
    json item;
    item["id"] = detail.id;
    item["name"] = detail.name;
    item["description"] = detail.description;
    item["image_url"] = detail.image_url;
    item["cooking_method"] = detail.cooking_method;
    item["flavor"] = detail.flavor;
    item["prep_time_minutes"] = detail.prep_time_minutes;
    item["cook_time_minutes"] = detail.cook_time_minutes;
    item["view_count"] = detail.view_count;
    item["avg_rating"] = detail.avg_rating;
    item["is_favorited"] = detail.is_favorited;
    json ingredients = json::array();
    for (const auto& ing : detail.ingredients)
        ingredients.push_back(toJson(ing));
    item["ingredients"] = ingredients;
    json steps = json::array();
    for (const auto& step : detail.steps)
        steps.push_back(toJson(step));
    item["steps"] = steps;
    item["nutrition"] = toJson(detail.nutrition);
    item["tags"] = detail.tags;
    item["author_id"] = detail.author_id;
    item["author_name"] = detail.author_name;
    item["created_at"] = detail.created_at;
    if (!detail.updated_at.empty())
        item["updated_at"] = detail.updated_at;
    return item;
}

json toJson(const RecipeVideo& video) {
    return {
        {"id", video.id},
        {"title", video.title},
        {"platform", video.platform},
        {"url", video.url},
        {"thumbnail_url", video.thumbnail_url},
        {"duration_seconds", video.duration_seconds}
    };
}

json toJson(const RecipeRating& rating) {
    return {
        {"id", rating.id},
        {"user_id", rating.user_id},
        {"username", rating.username},
        {"rating", rating.rating},
        {"comment", rating.comment},
        {"created_at", rating.created_at}
    };
}

json toJson(const MyRecipeStatus& status) {
    json item;
    item["id"] = status.id;
    item["name"] = status.name;
    item["status"] = status.status;
    if (status.reject_reason.has_value())
        item["reject_reason"] = status.reject_reason.value();
    item["submitted_at"] = status.submitted_at;
    if (!status.updated_at.empty())
        item["updated_at"] = status.updated_at;
    return item;
}

// ======================== 营养报告 ========================

json toJson(const NutritionBreakdownItem& item) {
    return {
        {"name", item.name},
        {"calories", item.calories},
        {"protein_g", item.protein_g},
        {"fat_g", item.fat_g},
        {"carbs_g", item.carbs_g}
    };
}

json toJson(const NutritionReport& report) {
    json j;
    j["recipe_id"] = report.recipe_id;
    j["recipe_name"] = report.recipe_name;
    j["per_serving"]["calories"] = report.per_serving.calories;
    j["per_serving"]["protein_g"] = report.per_serving.protein_g;
    j["per_serving"]["fat_g"] = report.per_serving.fat_g;
    j["per_serving"]["carbs_g"] = report.per_serving.carbs_g;
    j["per_serving"]["fiber_g"] = report.per_serving.fiber_g;
    j["per_serving"]["sodium_mg"] = report.per_serving.sodium_mg;
    j["per_serving"]["vitamin_c_mg"] = report.per_serving.vitamin_c_mg;
    json breakdown = json::array();
    for (const auto& item : report.ingredients_breakdown)
        breakdown.push_back(toJson(item));
    j["ingredients_breakdown"] = breakdown;
    json excluded = json::array();
    for (const auto& item : report.excluded_ingredients)
        excluded.push_back({{"name", item.name}, {"reason", item.reason}});
    j["excluded_ingredients"] = excluded;
    j["health_notes"] = report.health_notes;
    j["has_data"] = report.has_data;
    return j;
}

// ======================== 用户 / 认证相关 ========================

json toJson(const UserProfile& user) {
    return {
        {"id", user.id},
        {"username", user.username},
        {"display_name", user.display_name},
        {"email", user.email},
        {"phone", user.phone},
        {"avatar_url", user.avatar_url},
        {"preferences_complete", user.preferences_complete},
        {"created_at", user.created_at}
    };
}

json toJson(const UserPreferences& prefs) {
    return {{"likes", prefs.likes}, {"dislikes", prefs.dislikes}, {"health_goal", prefs.health_goal}};
}

json toJson(const HealthProfileResponse& resp) {
    json j;
    if (resp.height_cm.has_value())
        j["height_cm"] = resp.height_cm.value();
    if (resp.weight_kg.has_value())
        j["weight_kg"] = resp.weight_kg.value();
    if (!resp.conditions.empty())
        j["conditions"] = resp.conditions;
    json arr = json::array();
    for (const auto& item : resp.suggested_avoidances)
        arr.push_back(toJson(item));
    j["suggested_avoidances"] = arr;
    return j;
}

json toJson(const AvatarUploadResponse& resp) {
    return {
        {"avatar_id", resp.avatar_id},
        {"avatar_url", resp.avatar_url}
    };
}

json toJson(const FavoriteItem& item) {
    return {
        {"id", item.id},
        {"recipe_id", item.recipe_id},
        {"name", item.name},
        {"description", item.description},
        {"image_url", item.image_url},
        {"group_name", item.group_name},
        {"is_public", item.is_public},
        {"favorited_at", item.favorited_at}
    };
}

json toJson(const FavoriteGroup& group) {
    return {
        {"id", group.id},
        {"name", group.name},
        {"sort_order", group.sort_order},
        {"count", group.count}
    };
}

// ======================== 通知 / 我的评论 ========================

json toJson(const NotificationItem& item) {
    return {
        {"id", item.id},
        {"title", item.title},
        {"content", item.content},
        {"type", item.type},
        {"sub_type", item.sub_type},
        {"is_read", item.is_read},
        {"related_id", item.related_id},
        {"trigger_user_name", item.trigger_user_name},
        {"created_at", item.created_at}
    };
}

json toJson(const UserRatingItem& item) {
    return {
        {"rating_id", item.rating_id},
        {"recipe_id", item.recipe_id},
        {"recipe_name", item.recipe_name},
        {"rating", item.rating},
        {"comment", item.comment},
        {"created_at", item.created_at},
        {"updated_at", item.updated_at}
    };
}

// ======================== 库存 / 购物清单 ========================

json toJson(const InventoryItem& item) {
    json obj;
    obj["id"] = item.id;
    obj["ingredient_name"] = item.ingredient_name;
    obj["quantity"] = item.quantity;
    obj["unit"] = item.unit;
    if (item.expiry_date.has_value())
        obj["expiry_date"] = item.expiry_date.value();
    obj["added_at"] = item.added_at;
    return obj;
}

json toJson(const ShoppingListItem& item) {
    return {
        {"id", item.id},
        {"ingredient_name", item.ingredient_name},
        {"required_quantity", item.required_quantity},
        {"inventory_quantity", item.inventory_quantity},
        {"to_buy_quantity", item.to_buy_quantity},
        {"unit", item.unit},
        {"checked", item.checked}
    };
}

json toJson(const ShoppingListSummary& summary) {
    return {
        {"id", summary.id},
        {"name", summary.name},
        {"item_count", summary.item_count},
        {"created_at", summary.created_at}
    };
}

json toJson(const ShoppingList& list) {
    json obj;
    obj["id"] = list.id;
    obj["name"] = list.name;
    json items = json::array();
    for (const auto& item : list.items)
        items.push_back(toJson(item));
    obj["items"] = items;
    return obj;
}

json toJson(const BatchShoppingResponse& resp) {
    json obj;
    obj["message"] = resp.message;
    obj["items"] = json::array();
    for (const auto& item : resp.items)
        obj["items"].push_back(toJson(item));
    return obj;
}

// ======================== 膳食计划 ========================

json toJson(const MealPlanSummary& plan) {
    return {
        {"plan_id", plan.plan_id},
        {"date", plan.date},
        {"meal_type", plan.meal_type},
        {"recipe", toJson(plan.recipe)},
        {"nutrition", toJson(plan.nutrition)}
    };
}

json toJson(const DailyMealDetails& daily) {
    json obj;
    if (daily.breakfast.has_value()) obj["breakfast"] = toJson(daily.breakfast.value());
    if (daily.lunch.has_value())     obj["lunch"]     = toJson(daily.lunch.value());
    if (daily.dinner.has_value())    obj["dinner"]    = toJson(daily.dinner.value());
    if (daily.snack.has_value())     obj["snack"]     = toJson(daily.snack.value());
    return obj;
}

json toJson(const CalendarDay& day) {
    return {
        {"date", day.date},
        {"meals", toJson(day.meals)},
        {"daily_total", toJson(day.daily_total)}
    };
}

json toJson(const NutritionTrendItem& trend) {
    json j;
    j["date"] = trend.date;
    j["actual"] = toJson(trend.actual);
    j["recommended"] = toJson(trend.recommended);
    j["comparison"]["calories"]["diff"]       = trend.comparison.calories.diff;
    j["comparison"]["calories"]["percentage"] = trend.comparison.calories.percentage;
    j["comparison"]["protein"]["diff"]        = trend.comparison.protein.diff;
    j["comparison"]["protein"]["percentage"]  = trend.comparison.protein.percentage;
    j["comparison"]["fat"]["diff"]            = trend.comparison.fat.diff;
    j["comparison"]["fat"]["percentage"]      = trend.comparison.fat.percentage;
    j["comparison"]["carbs"]["diff"]          = trend.comparison.carbs.diff;
    j["comparison"]["carbs"]["percentage"]    = trend.comparison.carbs.percentage;
    return j;
}

// ======================== 公告 ========================

json toJson(const AnnouncementItem& ann) {
    return {
        {"id", ann.id},
        {"title", ann.title},
        {"content", ann.content},
        {"created_at", ann.created_at}
    };
}

// ======================== 管理员相关 ========================

json toJson(const AdminUser& user) {
    return {
        {"id", user.id},
        {"username", user.username},
        {"email", user.email},
        {"role", user.role},
        {"status", user.status},
        {"created_at", user.created_at}
    };
}

json toJson(const PendingRecipeItem& item) {
    return {
        {"id", item.id},
        {"name", item.name},
        {"author_id", item.author_id},
        {"author_name", item.author_name},
        {"submitted_at", item.submitted_at},
        {"status", item.status}
    };
}

json toJson(const BatchReviewResponse& resp) {
    return {
        {"message", resp.message},
        {"success_count", resp.success_count},
        {"failed_ids", resp.failed_ids}
    };
}

json toJson(const NotificationResponse& resp) {
    return {
        {"notification_id", resp.notification_id},
        {"status", resp.status},
        {"estimated_recipients", resp.estimated_recipients}
    };
}

} // namespace JsonSerializer
