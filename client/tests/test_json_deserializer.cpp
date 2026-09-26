// test_json_deserializer.cpp —— JsonDeserializer 解析层单测（纯内存，不依赖网络/数据库）
//
// 覆盖各批次迁移的解析函数（批次一：脚手架 + 菜谱域；批次二：用户/认证域）：
//   · parsePagination：合法 / 缺失（全 0）
//   · parseRecipeSummary：15 字段全量填充（含公开列表此前丢弃的 6 个属性字段）/ 缺字段默认值 / tags 非数组
//   · parseRecipeDetail：嵌套 ingredients/steps/tags；steps 可选 duration/image_url；
//     nutrition 缺省与 has_data 新旧服务端双分支（缺字段按四项值兜底）
//   · parseRecommendedRecipe：RecipeSummary 公共字段同源 + match_score/health_notice/match_status
//   · parseRecipeVideo / parseRecipeRating / parseSubmitRecipeResponse
//   · parseMyRecipeStatus（经 parsePagedMyRecipes）：reject_reason 缺失/null → nullopt
//   · parseNutritionReport：per_serving/breakdown/excluded；has_data 双分支（旧服务端按 per_serving 存在性兜底）
//   · 分页组合：parsePagedRecipes / parsePagedRecommendedRecipes（health_filter_applied）/ parsePagedRatings
//   · parseLoginResponse / parseUserProfile / parseUserPreferences / parseHealthProfileResponse（GET 全字段与 PUT 增量形状）/ parseAvatarUploadResponse
//   · parseFavoriteItem / parseFavoriteGroup / parseNotificationItem（可选字段 null vs 缺失）/ parseNotificationUnreadSummary（review/interaction/system 映射）/ parseAnnouncementItem
//   · parseInventoryItem（expiry_date null vs 缺失）/ parseShoppingListSummary / parseShoppingListItem / parseShoppingList / parseBatchShoppingResponse
//   · parseUserRatingItem（经 parsePagedUserRatings，含分页缺省口径 1/20）

#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "JsonDeserializer.h"

using namespace gocook::models;

namespace {

QJsonObject objFromJson(const char* text) {
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

} // namespace

// ==================== parsePagination ====================

TEST(JsonDeserializerTest, 分页元信息解析与缺省)
{
    const auto root = objFromJson(R"({"pagination":{"page":2,"size":20,"total":45,"total_pages":3}})");
    const Pagination p = JsonDeserializer::parsePagination(root);
    EXPECT_EQ(p.page, 2);
    EXPECT_EQ(p.size, 20);
    EXPECT_EQ(p.total, 45);
    EXPECT_EQ(p.total_pages, 3);

    const Pagination missing = JsonDeserializer::parsePagination(objFromJson(R"({})"));
    EXPECT_EQ(missing.page, 0);
    EXPECT_EQ(missing.size, 0);
    EXPECT_EQ(missing.total, 0);
    EXPECT_EQ(missing.total_pages, 0);
}

// ==================== parseRecipeSummary ====================

TEST(JsonDeserializerTest, 菜谱摘要全字段填充)
{
    const auto obj = objFromJson(R"({
        "id": 7,
        "name": "红烧肉",
        "description": "肥而不腻",
        "image_url": "/uploads/1.jpg",
        "cooking_method": "炖",
        "flavor": "咸鲜",
        "ingredient_type": "荤",
        "prep_time_minutes": 10,
        "cook_time_minutes": 60,
        "calories": 520,
        "view_count": 33,
        "avg_rating": 4.5,
        "author_id": 3,
        "author_name": "老王",
        "tags": ["家常菜", "宴客"]
    })");
    const RecipeSummary r = JsonDeserializer::parseRecipeSummary(obj);
    EXPECT_EQ(r.id, 7);
    EXPECT_EQ(r.name, "红烧肉");
    EXPECT_EQ(r.description, "肥而不腻");
    EXPECT_EQ(r.image_url, "/uploads/1.jpg");
    EXPECT_EQ(r.cooking_method, "炖") << "公开列表/搜索此前会丢弃 cooking_method，现统一填充";
    EXPECT_EQ(r.flavor, "咸鲜");
    EXPECT_EQ(r.ingredient_type, "荤");
    EXPECT_EQ(r.prep_time_minutes, 10);
    EXPECT_EQ(r.cook_time_minutes, 60);
    EXPECT_EQ(r.calories, 520);
    EXPECT_EQ(r.view_count, 33);
    EXPECT_DOUBLE_EQ(r.avg_rating, 4.5);
    EXPECT_EQ(r.author_id, 3);
    EXPECT_EQ(r.author_name, "老王");
    ASSERT_EQ(r.tags.size(), 2u);
    EXPECT_EQ(r.tags[0], "家常菜");
    EXPECT_EQ(r.tags[1], "宴客");
}

TEST(JsonDeserializerTest, 菜谱摘要缺字段回退默认值且tags非数组)
{
    const auto obj = objFromJson(R"({"id":1,"tags":"不是数组"})");
    const RecipeSummary r = JsonDeserializer::parseRecipeSummary(obj);
    EXPECT_EQ(r.id, 1);
    EXPECT_EQ(r.name, "");
    EXPECT_EQ(r.calories, 0);
    EXPECT_DOUBLE_EQ(r.avg_rating, 0.0);
    EXPECT_TRUE(r.tags.empty()) << "tags 非数组必须回退为空列表";

    const RecipeSummary empty = JsonDeserializer::parseRecipeSummary(objFromJson(R"({})"));
    EXPECT_EQ(empty.id, 0);
    EXPECT_TRUE(empty.tags.empty());
}

// ==================== parseRecipeDetail ====================

TEST(JsonDeserializerTest, 菜谱详情嵌套解析与步骤可选字段)
{
    const auto obj = objFromJson(R"({
        "id": 9,
        "name": "番茄炒蛋",
        "ingredients": [
            {"name": "番茄", "quantity": 2, "unit": "个"},
            {"name": "鸡蛋", "quantity": 3, "unit": "个"}
        ],
        "steps": [
            {"order": 1, "description": "切番茄", "duration": 120, "image_url": "/img/1.png"},
            {"order": 2, "description": "炒制"}
        ],
        "tags": ["快手"],
        "author_id": 5,
        "author_name": "小明",
        "is_favorited": true,
        "created_at": "2026-09-01T00:00:00Z",
        "updated_at": "2026-09-02T00:00:00Z"
    })");
    const RecipeDetail d = JsonDeserializer::parseRecipeDetail(obj);
    EXPECT_EQ(d.id, 9);
    EXPECT_EQ(d.name, "番茄炒蛋");
    ASSERT_EQ(d.ingredients.size(), 2u);
    EXPECT_EQ(d.ingredients[1].name, "鸡蛋");
    EXPECT_DOUBLE_EQ(d.ingredients[1].quantity, 3.0);
    ASSERT_EQ(d.steps.size(), 2u);
    ASSERT_TRUE(d.steps[0].duration.has_value());
    EXPECT_EQ(d.steps[0].duration.value(), 120);
    EXPECT_EQ(d.steps[0].image_url, "/img/1.png");
    EXPECT_FALSE(d.steps[1].duration.has_value()) << "缺 duration 应为 nullopt";
    EXPECT_EQ(d.steps[1].image_url, "");
    ASSERT_EQ(d.tags.size(), 1u);
    EXPECT_EQ(d.tags[0], "快手");
    EXPECT_TRUE(d.is_favorited);
    EXPECT_EQ(d.created_at, "2026-09-01T00:00:00Z");
    EXPECT_EQ(d.updated_at, "2026-09-02T00:00:00Z");
    EXPECT_FALSE(d.nutrition.has_data) << "缺 nutrition 块 → has_data=false";
}

TEST(JsonDeserializerTest, 详情营养has_data新旧服务端双分支)
{
    // 新服务端：has_data=false 为权威判定（即使四项值非 0）
    const auto modern = objFromJson(R"({
        "id": 1,
        "nutrition": {"calories": 100, "protein": 5, "fat": 3, "carbs": 8, "has_data": false}
    })");
    EXPECT_FALSE(JsonDeserializer::parseRecipeDetail(modern).nutrition.has_data);

    // 旧服务端：无 has_data 字段，按四项值兜底（任一 > 0 → true）
    const auto legacy = objFromJson(R"({
        "id": 1,
        "nutrition": {"calories": 100, "protein": 0, "fat": 0, "carbs": 0}
    })");
    EXPECT_TRUE(JsonDeserializer::parseRecipeDetail(legacy).nutrition.has_data);

    // 旧服务端全 0 → false
    const auto legacyZero = objFromJson(R"({
        "id": 1,
        "nutrition": {"calories": 0, "protein": 0, "fat": 0, "carbs": 0}
    })");
    EXPECT_FALSE(JsonDeserializer::parseRecipeDetail(legacyZero).nutrition.has_data);
}

// ==================== parseRecommendedRecipe ====================

TEST(JsonDeserializerTest, 推荐项公共字段同源且扩展字段解析)
{
    const auto obj = objFromJson(R"({
        "id": 11,
        "name": "清蒸鲈鱼",
        "cooking_method": "蒸",
        "flavor": "清淡",
        "ingredient_type": "海鲜",
        "calories": 220,
        "view_count": 9,
        "avg_rating": 4.8,
        "tags": ["低脂"],
        "match_score": 0.87,
        "health_notice": "含盐，高血压人群建议少盐清淡",
        "match_status": {
            "available_ingredients": [{"name": "鲈鱼", "quantity": 1, "unit": "条"}],
            "missing_ingredients": [{"name": "葱", "quantity": 2, "unit": "根"}]
        }
    })");
    const RecommendedRecipe rec = JsonDeserializer::parseRecommendedRecipe(obj);
    EXPECT_EQ(rec.name, "清蒸鲈鱼");
    EXPECT_EQ(rec.cooking_method, "蒸");
    EXPECT_EQ(rec.flavor, "清淡");
    EXPECT_EQ(rec.ingredient_type, "海鲜");
    EXPECT_EQ(rec.calories, 220);
    EXPECT_EQ(rec.view_count, 9);
    EXPECT_DOUBLE_EQ(rec.avg_rating, 4.8);
    ASSERT_EQ(rec.tags.size(), 1u);
    EXPECT_EQ(rec.tags[0], "低脂");
    EXPECT_DOUBLE_EQ(rec.match_score, 0.87);
    EXPECT_EQ(rec.health_notice, "含盐，高血压人群建议少盐清淡");
    ASSERT_EQ(rec.match_status.available_ingredients.size(), 1u);
    EXPECT_EQ(rec.match_status.available_ingredients[0].name, "鲈鱼");
    EXPECT_DOUBLE_EQ(rec.match_status.available_ingredients[0].quantity, 1.0);
    ASSERT_EQ(rec.match_status.missing_ingredients.size(), 1u);
    EXPECT_EQ(rec.match_status.missing_ingredients[0].unit, "根");
}

TEST(JsonDeserializerTest, 推荐项缺match_status回退空列表)
{
    const RecommendedRecipe rec = JsonDeserializer::parseRecommendedRecipe(objFromJson(R"({"id":2})"));
    EXPECT_EQ(rec.id, 2);
    EXPECT_TRUE(rec.match_status.available_ingredients.empty());
    EXPECT_TRUE(rec.match_status.missing_ingredients.empty());
    EXPECT_DOUBLE_EQ(rec.match_score, 0.0);
    EXPECT_EQ(rec.health_notice, "");
}

// ==================== 基础小 DTO ====================

TEST(JsonDeserializerTest, 视频评分投稿响应基础解析)
{
    const RecipeVideo v = JsonDeserializer::parseRecipeVideo(objFromJson(R"({
        "id": 3, "title": "教程", "platform": "bilibili", "url": "https://b23.tv/x",
        "thumbnail_url": "/t.png", "duration_seconds": 300
    })"));
    EXPECT_EQ(v.id, 3);
    EXPECT_EQ(v.title, "教程");
    EXPECT_EQ(v.platform, "bilibili");
    EXPECT_EQ(v.thumbnail_url, "/t.png");
    EXPECT_EQ(v.duration_seconds, 300);

    const RecipeRating r = JsonDeserializer::parseRecipeRating(objFromJson(R"({
        "id": 5, "user_id": 2, "username": "u", "rating": 4, "comment": "好吃", "created_at": "2026-09-10"
    })"));
    EXPECT_EQ(r.id, 5);
    EXPECT_EQ(r.user_id, 2);
    EXPECT_EQ(r.username, "u");
    EXPECT_EQ(r.rating, 4);
    EXPECT_EQ(r.comment, "好吃");
    EXPECT_EQ(r.created_at, "2026-09-10");

    const SubmitRecipeResponse s = JsonDeserializer::parseSubmitRecipeResponse(
        objFromJson(R"({"id": 42, "status": "pending"})"));
    EXPECT_EQ(s.id, 42);
    EXPECT_EQ(s.status, "pending");
}

// ==================== parseMyRecipeStatus（经分页组合） ====================

TEST(JsonDeserializerTest, 我的投稿reject_reason空值语义)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":20,"total":3,"total_pages":1},
        "data": [
            {"id":1,"name":"A","status":"pending","submitted_at":"t1","updated_at":"t1"},
            {"id":2,"name":"B","status":"rejected","reject_reason":null,"submitted_at":"t2","updated_at":"t2"},
            {"id":3,"name":"C","status":"rejected","reject_reason":"图片不清晰","submitted_at":"t3","updated_at":"t3"}
        ]
    })");
    const PagedMyRecipes paged = JsonDeserializer::parsePagedMyRecipes(root);
    ASSERT_EQ(paged.data.size(), 3u);
    EXPECT_EQ(paged.pagination.total, 3);
    EXPECT_FALSE(paged.data[0].reject_reason.has_value()) << "缺字段 → nullopt";
    EXPECT_FALSE(paged.data[1].reject_reason.has_value()) << "null → nullopt";
    ASSERT_TRUE(paged.data[2].reject_reason.has_value());
    EXPECT_EQ(paged.data[2].reject_reason.value(), "图片不清晰");
    EXPECT_EQ(paged.data[2].status, "rejected");
}

// ==================== parseNutritionReport ====================

TEST(JsonDeserializerTest, 营养报告解析与has_data双分支)
{
    const auto obj = objFromJson(R"({
        "recipe_id": 8,
        "recipe_name": "水煮鱼片",
        "per_serving": {"calories":350,"protein_g":32,"fat_g":18,"carbs_g":12,"fiber_g":2,"sodium_mg":950,"vitamin_c_mg":5},
        "ingredients_breakdown": [{"name":"草鱼","calories":200,"protein_g":20,"fat_g":8,"carbs_g":0}],
        "excluded_ingredients": [{"name":"花椒","reason":"未收录"}],
        "health_notes": "高钠提示",
        "has_data": true
    })");
    const NutritionReport report = JsonDeserializer::parseNutritionReport(obj);
    EXPECT_EQ(report.recipe_id, 8);
    EXPECT_EQ(report.recipe_name, "水煮鱼片");
    EXPECT_DOUBLE_EQ(report.per_serving.calories, 350.0);
    EXPECT_DOUBLE_EQ(report.per_serving.sodium_mg, 950.0);
    ASSERT_EQ(report.ingredients_breakdown.size(), 1u);
    EXPECT_EQ(report.ingredients_breakdown[0].name, "草鱼");
    ASSERT_EQ(report.excluded_ingredients.size(), 1u);
    EXPECT_EQ(report.excluded_ingredients[0].reason, "未收录");
    EXPECT_EQ(report.health_notes, "高钠提示");
    EXPECT_TRUE(report.has_data);

    // 新服务端 has_data=false 为权威判定
    const auto modernFalse = objFromJson(R"({"recipe_id":8,"has_data":false})");
    EXPECT_FALSE(JsonDeserializer::parseNutritionReport(modernFalse).has_data);

    // 旧服务端无 has_data：按 per_serving 存在性兜底
    const auto legacy = objFromJson(R"({"recipe_id":8,"per_serving":{"calories":1}})");
    EXPECT_TRUE(JsonDeserializer::parseNutritionReport(legacy).has_data);

    const auto legacyMissing = objFromJson(R"({"recipe_id":8})");
    EXPECT_FALSE(JsonDeserializer::parseNutritionReport(legacyMissing).has_data);
}

// ==================== 分页组合 ====================

TEST(JsonDeserializerTest, 分页菜谱与推荐列表组合解析)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":20,"total":1,"total_pages":1},
        "data": [{"id":1,"name":"A","calories":100}]
    })");
    const PagedRecipes paged = JsonDeserializer::parsePagedRecipes(root);
    EXPECT_EQ(paged.pagination.total, 1);
    ASSERT_EQ(paged.data.size(), 1u);
    EXPECT_EQ(paged.data[0].calories, 100);

    const PagedRecipes empty = JsonDeserializer::parsePagedRecipes(objFromJson(R"({})"));
    EXPECT_TRUE(empty.data.empty());
    EXPECT_EQ(empty.pagination.page, 0);

    const auto recRoot = objFromJson(R"({
        "health_filter_applied": true,
        "pagination": {"page":1,"size":5,"total":1,"total_pages":1},
        "data": [{"id":2,"name":"B","match_score":0.5}]
    })");
    const PagedRecommendedRecipes recs = JsonDeserializer::parsePagedRecommendedRecipes(recRoot);
    EXPECT_TRUE(recs.health_filter_applied);
    ASSERT_EQ(recs.data.size(), 1u);
    EXPECT_EQ(recs.data[0].name, "B");
    EXPECT_DOUBLE_EQ(recs.data[0].match_score, 0.5);

    const PagedRecommendedRecipes recDefault = JsonDeserializer::parsePagedRecommendedRecipes(objFromJson(R"({})"));
    EXPECT_FALSE(recDefault.health_filter_applied);
    EXPECT_TRUE(recDefault.data.empty());
}

TEST(JsonDeserializerTest, 分页评分解析)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":10,"total":2,"total_pages":1},
        "data": [
            {"id":1,"user_id":1,"username":"a","rating":5,"comment":"赞","created_at":"t"},
            {"id":2,"user_id":2,"username":"b","rating":3,"comment":"","created_at":"t"}
        ]
    })");
    const PagedRatings paged = JsonDeserializer::parsePagedRatings(root);
    ASSERT_EQ(paged.data.size(), 2u);
    EXPECT_EQ(paged.data[0].rating, 5);
    EXPECT_EQ(paged.data[1].username, "b");
    EXPECT_EQ(paged.data[1].comment, "");
}

// ==================== parseLoginResponse / parseUserProfile ====================

TEST(JsonDeserializerTest, 登录响应与用户资料解析)
{
    const LoginResponse lr = JsonDeserializer::parseLoginResponse(
        objFromJson(R"({"token":"jwt.x","user_id":12,"username":"u1"})"));
    EXPECT_EQ(lr.token, "jwt.x");
    EXPECT_EQ(lr.user_id, 12);
    EXPECT_EQ(lr.username, "u1");

    const UserProfile p = JsonDeserializer::parseUserProfile(objFromJson(R"({
        "id": 12, "username": "u1", "display_name": "小明", "email": "a@b.c",
        "phone": "13800000000", "avatar_url": "/uploads/a.jpg",
        "preferences_complete": true, "created_at": "2026-09-01T00:00:00Z"
    })"));
    EXPECT_EQ(p.id, 12);
    EXPECT_EQ(p.display_name, "小明");
    EXPECT_EQ(p.email, "a@b.c");
    EXPECT_EQ(p.phone, "13800000000");
    EXPECT_EQ(p.avatar_url, "/uploads/a.jpg");
    EXPECT_TRUE(p.preferences_complete);
    EXPECT_EQ(p.created_at, "2026-09-01T00:00:00Z");

    const UserProfile empty = JsonDeserializer::parseUserProfile(objFromJson(R"({})"));
    EXPECT_EQ(empty.id, 0);
    EXPECT_FALSE(empty.preferences_complete);
}

// ==================== parseUserPreferences ====================

TEST(JsonDeserializerTest, 饮食偏好解析与缺省)
{
    const UserPreferences prefs = JsonDeserializer::parseUserPreferences(objFromJson(R"({
        "likes": ["辣", "海鲜"], "dislikes": ["香菜"], "health_goal": "减脂"
    })"));
    ASSERT_EQ(prefs.likes.size(), 2u);
    EXPECT_EQ(prefs.likes[1], "海鲜");
    ASSERT_EQ(prefs.dislikes.size(), 1u);
    EXPECT_EQ(prefs.dislikes[0], "香菜");
    EXPECT_EQ(prefs.health_goal, "减脂");

    const UserPreferences empty = JsonDeserializer::parseUserPreferences(objFromJson(R"({})"));
    EXPECT_TRUE(empty.likes.empty());
    EXPECT_TRUE(empty.dislikes.empty());
    EXPECT_EQ(empty.health_goal, "");
}

// ==================== parseHealthProfileResponse / parseAvatarUploadResponse ====================

TEST(JsonDeserializerTest, 健康指标响应全字段与增量响应)
{
    // GET 全字段
    const HealthProfileResponse full = JsonDeserializer::parseHealthProfileResponse(objFromJson(R"({
        "height_cm": 180, "weight_kg": 75.5,
        "conditions": ["高血压"],
        "suggested_avoidances": [{"ingredient": "盐", "reason": "高血压"}]
    })"));
    ASSERT_TRUE(full.height_cm.has_value());
    EXPECT_EQ(full.height_cm.value(), 180);
    ASSERT_TRUE(full.weight_kg.has_value());
    EXPECT_DOUBLE_EQ(full.weight_kg.value(), 75.5);
    ASSERT_EQ(full.conditions.size(), 1u);
    EXPECT_EQ(full.conditions[0], "高血压");
    ASSERT_EQ(full.suggested_avoidances.size(), 1u);
    EXPECT_EQ(full.suggested_avoidances[0].ingredient, "盐");

    // PUT 增量形状（仅 avoidances）：其余字段缺省保持 nullopt/空
    const HealthProfileResponse inc = JsonDeserializer::parseHealthProfileResponse(objFromJson(R"({
        "suggested_avoidances": [{"ingredient": "糖", "reason": "血糖"}]
    })"));
    EXPECT_FALSE(inc.height_cm.has_value());
    EXPECT_FALSE(inc.weight_kg.has_value());
    EXPECT_TRUE(inc.conditions.empty());
    ASSERT_EQ(inc.suggested_avoidances.size(), 1u);
    EXPECT_EQ(inc.suggested_avoidances[0].reason, "血糖");
}

TEST(JsonDeserializerTest, 头像上传响应与缺失url)
{
    const AvatarUploadResponse ok = JsonDeserializer::parseAvatarUploadResponse(
        objFromJson(R"({"avatar_url":"/uploads/tmp/u1.jpg"})"));
    EXPECT_EQ(ok.avatar_url, "/uploads/tmp/u1.jpg");

    // 缺字段 → 空串（调用点据此判"无效的响应格式"）
    const AvatarUploadResponse empty = JsonDeserializer::parseAvatarUploadResponse(objFromJson(R"({})"));
    EXPECT_EQ(empty.avatar_url, "");
}

// ==================== 收藏 / 通知 / 公告 ====================

TEST(JsonDeserializerTest, 收藏项与分组解析)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":20,"total":1,"total_pages":1},
        "data": [{
            "id": 3, "recipe_id": 7, "name": "红烧肉", "description": "肥而不腻",
            "image_url": "/1.jpg", "group_name": "家常", "is_public": true,
            "favorited_at": "2026-09-01"
        }]
    })");
    const PagedFavorites paged = JsonDeserializer::parsePagedFavorites(root);
    ASSERT_EQ(paged.data.size(), 1u);
    EXPECT_EQ(paged.data[0].id, 3);
    EXPECT_EQ(paged.data[0].recipe_id, 7);
    EXPECT_EQ(paged.data[0].group_name, "家常");
    EXPECT_TRUE(paged.data[0].is_public);
    EXPECT_EQ(paged.data[0].favorited_at, "2026-09-01");

    const FavoriteGroup g = JsonDeserializer::parseFavoriteGroup(
        objFromJson(R"({"id":2,"name":"家常","sort_order":1,"count":5})"));
    EXPECT_EQ(g.id, 2);
    EXPECT_EQ(g.name, "家常");
    EXPECT_EQ(g.sort_order, 1);
    EXPECT_EQ(g.count, 5);
}

TEST(JsonDeserializerTest, 通知项可选字段空值语义与未读汇总映射)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":20,"total":2,"total_pages":1},
        "data": [
            {"id":1,"title":"审核","content":"通过","type":"review","is_read":false,"created_at":"t1"},
            {"id":2,"title":"互动","content":"赞","type":"interaction","sub_type":"like","is_read":true,
             "related_id":null,"trigger_user_name":"小明","created_at":"t2"}
        ]
    })");
    const PagedNotifications paged = JsonDeserializer::parsePagedNotifications(root);
    ASSERT_EQ(paged.data.size(), 2u);
    EXPECT_EQ(paged.data[0].sub_type, "") << "sub_type 为非 optional 字符串，缺失 → 空串";
    EXPECT_FALSE(paged.data[0].related_id.has_value()) << "缺字段 → nullopt";
    EXPECT_FALSE(paged.data[0].trigger_user_name.has_value());
    EXPECT_EQ(paged.data[1].sub_type, "like");
    EXPECT_FALSE(paged.data[1].related_id.has_value()) << "null → nullopt";
    ASSERT_TRUE(paged.data[1].trigger_user_name.has_value());
    EXPECT_EQ(paged.data[1].trigger_user_name.value(), "小明");
    EXPECT_TRUE(paged.data[1].is_read);

    const NotificationUnreadSummary s = JsonDeserializer::parseNotificationUnreadSummary(
        objFromJson(R"({"review":3,"interaction":2,"system":true})"));
    EXPECT_EQ(s.unread_review, 3);
    EXPECT_EQ(s.unread_interaction, 2);
    EXPECT_TRUE(s.has_new_announcement);

    const NotificationUnreadSummary empty = JsonDeserializer::parseNotificationUnreadSummary(objFromJson(R"({})"));
    EXPECT_EQ(empty.unread_review, 0);
    EXPECT_EQ(empty.unread_interaction, 0);
    EXPECT_FALSE(empty.has_new_announcement);
}

TEST(JsonDeserializerTest, 公告分页解析)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":10,"total":1,"total_pages":1},
        "data": [{"id":9,"title":"维护通知","content":"今晚维护","created_at":"2026-09-20"}]
    })");
    const PagedAnnouncements paged = JsonDeserializer::parsePagedAnnouncements(root);
    ASSERT_EQ(paged.data.size(), 1u);
    EXPECT_EQ(paged.data[0].id, 9);
    EXPECT_EQ(paged.data[0].title, "维护通知");
    EXPECT_EQ(paged.data[0].created_at, "2026-09-20");
}

// ==================== 库存 / 购物清单 ====================

TEST(JsonDeserializerTest, 库存项expiry空值语义与分页解析)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":20,"total":2,"total_pages":1},
        "data": [
            {"id":1,"ingredient_name":"米","quantity":5,"unit":"千克","added_at":"t1"},
            {"id":2,"ingredient_name":"油","quantity":1,"unit":"升","expiry_date":null,"added_at":"t2"}
        ]
    })");
    const PagedInventory paged = JsonDeserializer::parsePagedInventory(root);
    ASSERT_EQ(paged.data.size(), 2u);
    EXPECT_FALSE(paged.data[0].expiry_date.has_value()) << "缺字段 → nullopt";
    EXPECT_FALSE(paged.data[1].expiry_date.has_value()) << "null → nullopt";
    EXPECT_DOUBLE_EQ(paged.data[0].quantity, 5.0);
    EXPECT_EQ(paged.data[0].unit, "千克");

    const InventoryItem withDate = JsonDeserializer::parseInventoryItem(
        objFromJson(R"({"id":3,"expiry_date":"2026-10-01"})"));
    ASSERT_TRUE(withDate.expiry_date.has_value());
    EXPECT_EQ(withDate.expiry_date.value(), "2026-10-01");
}

TEST(JsonDeserializerTest, 购物清单与批量响应解析)
{
    const ShoppingList list = JsonDeserializer::parseShoppingList(objFromJson(R"({
        "id": 4, "name": "周末采购",
        "items": [
            {"id":1,"ingredient_name":"番茄","required_quantity":3,"inventory_quantity":1,"to_buy_quantity":2,"unit":"个","checked":false},
            {"id":2,"ingredient_name":"鸡蛋","required_quantity":6,"inventory_quantity":6,"to_buy_quantity":0,"unit":"个","checked":true}
        ]
    })"));
    EXPECT_EQ(list.id, 4);
    EXPECT_EQ(list.name, "周末采购");
    ASSERT_EQ(list.items.size(), 2u);
    EXPECT_DOUBLE_EQ(list.items[0].to_buy_quantity, 2.0);
    EXPECT_EQ(list.items[0].unit, "个");
    EXPECT_TRUE(list.items[1].checked);

    // items 缺失 → 空列表
    const ShoppingList empty = JsonDeserializer::parseShoppingList(objFromJson(R"({"id":5,"name":"空单"})"));
    EXPECT_TRUE(empty.items.empty());

    const BatchShoppingResponse resp = JsonDeserializer::parseBatchShoppingResponse(objFromJson(R"({
        "message": "已成功添加 1 项",
        "items": [{"id":9,"ingredient_name":"盐","required_quantity":1,"inventory_quantity":0,"to_buy_quantity":1,"unit":"克","checked":false}]
    })"));
    EXPECT_EQ(resp.message, "已成功添加 1 项");
    ASSERT_EQ(resp.items.size(), 1u);
    EXPECT_EQ(resp.items[0].ingredient_name, "盐");

    const ShoppingListSummary summary = JsonDeserializer::parseShoppingListSummary(
        objFromJson(R"({"id":6,"name":"清单","item_count":3,"created_at":"2026-09-01"})"));
    EXPECT_EQ(summary.id, 6);
    EXPECT_EQ(summary.item_count, 3);
    EXPECT_EQ(summary.created_at, "2026-09-01");
}

// ==================== 我的评论（UserRatingItem） ====================

TEST(JsonDeserializerTest, 我的评论分页解析与分页缺省口径)
{
    const auto root = objFromJson(R"({
        "pagination": {"page":1,"size":10,"total":1,"total_pages":1},
        "data": [{"rating_id":5,"recipe_id":7,"recipe_name":"红烧肉","rating":4,"comment":"好吃","created_at":"t1","updated_at":"t2"}]
    })");
    const PagedUserRatings paged = JsonDeserializer::parsePagedUserRatings(root);
    ASSERT_EQ(paged.data.size(), 1u);
    EXPECT_EQ(paged.data[0].rating_id, 5);
    EXPECT_EQ(paged.data[0].recipe_name, "红烧肉");
    EXPECT_EQ(paged.data[0].rating, 4);
    EXPECT_EQ(paged.data[0].updated_at, "t2");

    // 分页缺省口径：page=1 / size=20（历史行为，与通用 parsePaged 的 0 缺省不同）
    const PagedUserRatings empty = JsonDeserializer::parsePagedUserRatings(objFromJson(R"({})"));
    EXPECT_TRUE(empty.data.empty());
    EXPECT_EQ(empty.pagination.page, 1);
    EXPECT_EQ(empty.pagination.size, 20);
}
