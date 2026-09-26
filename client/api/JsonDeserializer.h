#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <gocook/DataModels.h>

/**
 * @brief API 响应 JSON → C++ 业务模型（gocook::models::*）的反序列化层。
 *
 * 按 DTO 为最小单位组织（一个 DTO 一个函数）；分页等组合类型在基础 DTO 函数之上组合，
 * 不重复解析公共字段。与 DataMapper（模型 → QVariantMap 供 QML）形成读写对称：
 * 网络层（HttpGoCookApi）只负责传输/鉴权/错误归一，不再逐字段拼装 DTO。
 *
 * 语义约定（与迁移前各调用点行为逐字等价，由 test_json_deserializer.cpp 锁定）：
 *  - 宽容读取：字段缺失/类型不符按 QJson 默认值回退（0 / 空串 / 空数组），不抛异常；
 *  - 可选字段（std::optional）：字段缺失或为 null 时保持 nullopt；
 *  - has_data 旧服务端兜底：缺字段时按数据存在性回退（详见各函数内注释）。
 */
namespace JsonDeserializer {

// ========== 基础 ==========
/// 读取 { pagination: { page, size, total, total_pages } }（root 缺失时全 0）
gocook::models::Pagination parsePagination(const QJsonObject& root);

// ========== 菜谱相关 ==========
gocook::models::Ingredient parseIngredient(const QJsonObject& obj);
gocook::models::CookingStep parseCookingStep(const QJsonObject& obj);
/// 营养块（含 has_data 旧服务端四项值兜底）
gocook::models::Nutrition parseNutrition(const QJsonObject& nut);
/// 菜谱摘要：填充 RecipeSummary 全部 15 个字段（公开列表/搜索/推荐三处共用同一实现）
gocook::models::RecipeSummary parseRecipeSummary(const QJsonObject& obj);
gocook::models::RecipeDetail parseRecipeDetail(const QJsonObject& obj);
/// 推荐项：RecipeSummary 公共字段（同 parseRecipeSummary）+ match_score / health_notice / match_status
gocook::models::RecommendedRecipe parseRecommendedRecipe(const QJsonObject& obj);
gocook::models::RecipeVideo parseRecipeVideo(const QJsonObject& obj);
gocook::models::RecipeRating parseRecipeRating(const QJsonObject& obj);
gocook::models::MyRecipeStatus parseMyRecipeStatus(const QJsonObject& obj);
gocook::models::SubmitRecipeResponse parseSubmitRecipeResponse(const QJsonObject& obj);
/// 独立营养报告（含 has_data / per_serving 旧服务端兜底）
gocook::models::NutritionReport parseNutritionReport(const QJsonObject& obj);

// ========== 用户 / 认证相关 ==========
gocook::models::LoginResponse parseLoginResponse(const QJsonObject& obj);
gocook::models::UserProfile parseUserProfile(const QJsonObject& obj);
gocook::models::UserPreferences parseUserPreferences(const QJsonObject& obj);
/// 健康指标响应（GET / PUT 共用；按字段存在性宽容读取，PUT 增量响应缺省字段保持 nullopt/空）
gocook::models::HealthProfileResponse parseHealthProfileResponse(const QJsonObject& obj);
/// 头像上传响应（仅读取 avatar_url；"缺失即无效响应"判定留在调用点）
gocook::models::AvatarUploadResponse parseAvatarUploadResponse(const QJsonObject& obj);

// ========== 收藏 / 通知 / 公告 ==========
gocook::models::FavoriteItem parseFavoriteItem(const QJsonObject& obj);
gocook::models::FavoriteGroup parseFavoriteGroup(const QJsonObject& obj);
gocook::models::NotificationItem parseNotificationItem(const QJsonObject& obj);
/// 未读汇总：线格式字段 review / interaction / system 映射到 DTO 的 unread_review / unread_interaction / has_new_announcement
gocook::models::NotificationUnreadSummary parseNotificationUnreadSummary(const QJsonObject& obj);
gocook::models::AnnouncementItem parseAnnouncementItem(const QJsonObject& obj);
gocook::models::UserRatingItem parseUserRatingItem(const QJsonObject& obj);

// ========== 库存 / 购物清单 ==========
gocook::models::InventoryItem parseInventoryItem(const QJsonObject& obj);
gocook::models::ShoppingListSummary parseShoppingListSummary(const QJsonObject& obj);
gocook::models::ShoppingListItem parseShoppingListItem(const QJsonObject& obj);
gocook::models::ShoppingList parseShoppingList(const QJsonObject& obj);
gocook::models::BatchShoppingResponse parseBatchShoppingResponse(const QJsonObject& obj);

// ========== 分页组合（基础 DTO 函数之上组合，含分页元信息） ==========
gocook::models::PagedRecipes parsePagedRecipes(const QJsonObject& root);
gocook::models::PagedRecommendedRecipes parsePagedRecommendedRecipes(const QJsonObject& root);
gocook::models::PagedRatings parsePagedRatings(const QJsonObject& root);
gocook::models::PagedMyRecipes parsePagedMyRecipes(const QJsonObject& root);
gocook::models::PagedFavorites parsePagedFavorites(const QJsonObject& root);
gocook::models::PagedNotifications parsePagedNotifications(const QJsonObject& root);
gocook::models::PagedAnnouncements parsePagedAnnouncements(const QJsonObject& root);
gocook::models::PagedInventory parsePagedInventory(const QJsonObject& root);
gocook::models::PagedUserRatings parsePagedUserRatings(const QJsonObject& root);

} // namespace JsonDeserializer
