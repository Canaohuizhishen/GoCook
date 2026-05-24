#pragma once

#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <gocook/DataModels.h>

/**
 * @brief 数据模型映射工具类
 *
 * 将 C++ 业务模型结构体（gocook::models::*）统一转换为 QVariantMap，
 * 隔离转换逻辑，避免在 ViewModel 回调中手工逐个字段赋值。
 * 当模型字段增减时，只需在此处更新映射，所有调用方自动受益。
 */
namespace DataMapper {

    // ========== 基础模型 ==========
    QVariantMap toMap(const gocook::models::Pagination& pagination);
    QVariantMap toMap(const gocook::models::Nutrition& nutrition);
    QVariantMap toMap(const gocook::models::Ingredient& ingredient);
    QVariantMap toMap(const gocook::models::CookingStep& step);
    QVariantMap toMap(const gocook::models::AvoidanceItem& item);

    // ========== 菜谱相关 ==========
    QVariantMap toMap(const gocook::models::RecipeSummary& recipe);
    QVariantMap toMap(const gocook::models::RecommendedRecipe& recipe);
    QVariantMap toMap(const gocook::models::RecipeDetail& detail);
    QVariantMap toMap(const gocook::models::RecipeVideo& video);
    QVariantMap toMap(const gocook::models::SubmitRecipeResponse& resp);

    // ========== 营养报告（v2.8 新增） ==========
    QVariantMap toMap(const gocook::models::NutritionBreakdownItem& item);
    QVariantMap toMap(const gocook::models::NutritionReport& report);

    // ========== 用户 / 认证相关 ==========
    QVariantMap toMap(const gocook::models::FavoriteItem& item);
    QVariantMap toMap(const gocook::models::RecipeRating& rating);
    QVariantMap toMap(const gocook::models::MyRecipeStatus& status);
    QVariantMap toMap(const gocook::models::UserProfile& user);
    QVariantMap toMap(const gocook::models::LoginResponse& login);
    QVariantMap toMap(const gocook::models::HealthProfileResponse& health);

    // ========== 通知 / 我的评论 ==========
    QVariantMap toMap(const gocook::models::NotificationItem& item);
    QVariantMap toMap(const gocook::models::UserRatingItem& item);

    // ========== 库存 / 购物清单相关 ==========
    QVariantMap toMap(const gocook::models::InventoryItem& item);
    QVariantMap toMap(const gocook::models::ShoppingListItem& item);
    QVariantMap toMap(const gocook::models::ShoppingListSummary& summary);
    QVariantMap toMap(const gocook::models::ShoppingList& list);

    // ========== 膳食计划相关 ==========
    QVariantMap toMap(const gocook::models::MealPlanSummary& plan);
    QVariantMap toMap(const gocook::models::DailyMealDetails& daily);
    QVariantMap toMap(const gocook::models::CalendarDay& day);
    QVariantMap toMap(const gocook::models::NutritionTrendItem& trend);

    // ========== 公告相关 ==========
    QVariantMap toMap(const gocook::models::AnnouncementItem& ann);
    /// 将公告项映射为通知格式（附加 is_read=true, type="system"），
    /// 使公告可直接在消息通知页的"系统公告"标签中展示，无需为每个用户创建副本。
    QVariantMap toNotificationMap(const gocook::models::AnnouncementItem& ann);

    // ========== 分页结果 ==========
    QVariantMap toMap(const gocook::models::PagedRecipes& paged);
    QVariantMap toMap(const gocook::models::PagedRecommendedRecipes& paged);
    QVariantMap toMap(const gocook::models::PagedFavorites& paged);
    QVariantMap toMap(const gocook::models::PagedInventory& paged);
    QVariantMap toMap(const gocook::models::PagedRatings& paged);
    QVariantMap toMap(const gocook::models::PagedAnnouncements& paged);
    QVariantMap toMap(const gocook::models::PagedMyRecipes& paged);
    QVariantMap toMap(const gocook::models::PagedUsers& paged);
    QVariantMap toMap(const gocook::models::PagedMealPlans& paged);
    QVariantMap toMap(const gocook::models::PagedPendingRecipes& paged);
    QVariantMap toMap(const gocook::models::PagedNotifications& paged);
    QVariantMap toMap(const gocook::models::PagedUserRatings& paged);
    QVariantMap toMap(const gocook::models::PagedCalendarDays& paged);

} // namespace DataMapper