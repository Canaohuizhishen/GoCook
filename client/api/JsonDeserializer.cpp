#include "JsonDeserializer.h"

namespace JsonDeserializer {

using namespace gocook::models;

// ======================== 基础 ========================

Pagination parsePagination(const QJsonObject& root) {
    Pagination pagination;
    const QJsonObject pag = root["pagination"].toObject();
    pagination.page        = pag["page"].toInt();
    pagination.size        = pag["size"].toInt();
    pagination.total       = pag["total"].toInt();
    pagination.total_pages = pag["total_pages"].toInt();
    return pagination;
}

// ======================== 菜谱相关 ========================

Ingredient parseIngredient(const QJsonObject& obj) {
    Ingredient ing;
    ing.name     = obj["name"].toString().toStdString();
    ing.quantity = obj["quantity"].toDouble();
    ing.unit     = obj["unit"].toString().toStdString();
    return ing;
}

CookingStep parseCookingStep(const QJsonObject& obj) {
    CookingStep step;
    step.order       = obj["order"].toInt();
    step.description = obj["description"].toString().toStdString();
    if (obj.contains("duration"))
        step.duration = obj["duration"].toInt();
    if (obj.contains("image_url"))
        step.image_url = obj["image_url"].toString().toStdString();
    return step;
}

Nutrition parseNutrition(const QJsonObject& nut) {
    Nutrition nutrition;
    nutrition.calories = nut["calories"].toDouble();
    nutrition.protein  = nut["protein"].toDouble();
    nutrition.fat      = nut["fat"].toDouble();
    nutrition.carbs    = nut["carbs"].toDouble();
    if (nut.contains("has_data")) {
        // 新服务端：has_data 为权威判定（false=无营养报告，前端展示空态而非全 0）
        nutrition.has_data = nut["has_data"].toBool();
    } else {
        // 旧服务端未返回该字段：按四项值兜底（等价于修复前 QML 的 calories>0 判定，
        // 避免无数据菜谱的"营养合计"卡片显示全 0）
        nutrition.has_data = nutrition.calories > 0 || nutrition.protein > 0
            || nutrition.fat > 0 || nutrition.carbs > 0;
    }
    return nutrition;
}

RecipeSummary parseRecipeSummary(const QJsonObject& obj) {
    RecipeSummary recipe;
    recipe.id                = obj["id"].toInt();
    recipe.name              = obj["name"].toString().toStdString();
    recipe.description       = obj["description"].toString().toStdString();
    recipe.image_url         = obj["image_url"].toString().toStdString();
    recipe.cooking_method    = obj["cooking_method"].toString().toStdString();
    recipe.flavor            = obj["flavor"].toString().toStdString();
    recipe.ingredient_type   = obj["ingredient_type"].toString().toStdString();
    recipe.prep_time_minutes = obj["prep_time_minutes"].toInt();
    recipe.cook_time_minutes = obj["cook_time_minutes"].toInt();
    recipe.calories          = obj["calories"].toInt();
    recipe.view_count        = obj["view_count"].toInt();
    recipe.avg_rating        = obj["avg_rating"].toDouble();
    recipe.author_id         = obj["author_id"].toInt();
    recipe.author_name       = obj["author_name"].toString().toStdString();
    if (obj.contains("tags") && obj["tags"].isArray()) {
        const QJsonArray tagsArr = obj["tags"].toArray();
        for (const QJsonValue& tag : tagsArr)
            recipe.tags.push_back(tag.toString().toStdString());
    }
    return recipe;
}

RecipeDetail parseRecipeDetail(const QJsonObject& obj) {
    RecipeDetail detail;
    detail.id                = obj["id"].toInt();
    detail.name              = obj["name"].toString().toStdString();
    detail.description       = obj["description"].toString().toStdString();
    detail.image_url         = obj["image_url"].toString().toStdString();
    detail.cooking_method    = obj["cooking_method"].toString().toStdString();
    detail.flavor            = obj["flavor"].toString().toStdString();
    detail.prep_time_minutes = obj["prep_time_minutes"].toInt();
    detail.cook_time_minutes = obj["cook_time_minutes"].toInt();
    detail.view_count        = obj["view_count"].toInt();
    detail.avg_rating        = obj["avg_rating"].toDouble();
    if (obj.contains("ingredients") && obj["ingredients"].isArray()) {
        for (const QJsonValue& val : obj["ingredients"].toArray())
            detail.ingredients.push_back(parseIngredient(val.toObject()));
    }
    if (obj.contains("steps") && obj["steps"].isArray()) {
        for (const QJsonValue& val : obj["steps"].toArray())
            detail.steps.push_back(parseCookingStep(val.toObject()));
    }
    if (obj.contains("nutrition") && obj["nutrition"].isObject())
        detail.nutrition = parseNutrition(obj["nutrition"].toObject());
    if (obj.contains("tags") && obj["tags"].isArray()) {
        for (const QJsonValue& val : obj["tags"].toArray())
            detail.tags.push_back(val.toString().toStdString());
    }
    detail.author_id    = obj["author_id"].toInt();
    detail.author_name  = obj["author_name"].toString().toStdString();
    detail.is_favorited = obj["is_favorited"].toBool();
    detail.created_at   = obj["created_at"].toString().toStdString();
    detail.updated_at   = obj["updated_at"].toString().toStdString();
    return detail;
}

RecommendedRecipe parseRecommendedRecipe(const QJsonObject& obj) {
    RecommendedRecipe rec;
    // RecipeSummary 公共字段：与公开列表/搜索同源（RecommendedRecipe 继承 RecipeSummary）
    static_cast<RecipeSummary&>(rec) = parseRecipeSummary(obj);

    // RecommendedRecipe 扩展字段
    rec.match_score    = obj["match_score"].toDouble();
    rec.health_notice  = obj["health_notice"].toString().toStdString();

    const QJsonObject status = obj["match_status"].toObject();
    if (status.contains("available_ingredients") && status["available_ingredients"].isArray()) {
        for (const QJsonValue& v : status["available_ingredients"].toArray()) {
            const QJsonObject ing = v.toObject();
            MatchIngredient mi;
            mi.name     = ing["name"].toString().toStdString();
            mi.quantity = ing["quantity"].toDouble();
            mi.unit     = ing["unit"].toString().toStdString();
            rec.match_status.available_ingredients.push_back(std::move(mi));
        }
    }
    if (status.contains("missing_ingredients") && status["missing_ingredients"].isArray()) {
        for (const QJsonValue& v : status["missing_ingredients"].toArray()) {
            const QJsonObject ing = v.toObject();
            MissingIngredient mi;
            mi.name     = ing["name"].toString().toStdString();
            mi.quantity = ing["quantity"].toDouble();
            mi.unit     = ing["unit"].toString().toStdString();
            rec.match_status.missing_ingredients.push_back(std::move(mi));
        }
    }
    return rec;
}

RecipeVideo parseRecipeVideo(const QJsonObject& obj) {
    RecipeVideo v;
    v.id               = obj["id"].toInt();
    v.title            = obj["title"].toString().toStdString();
    v.platform         = obj["platform"].toString().toStdString();
    v.url              = obj["url"].toString().toStdString();
    v.thumbnail_url    = obj["thumbnail_url"].toString().toStdString();
    v.duration_seconds = obj["duration_seconds"].toInt();
    return v;
}

RecipeRating parseRecipeRating(const QJsonObject& obj) {
    RecipeRating r;
    r.id         = obj["id"].toInt();
    r.user_id    = obj["user_id"].toInt();
    r.username   = obj["username"].toString().toStdString();
    r.rating     = obj["rating"].toInt();
    r.comment    = obj["comment"].toString().toStdString();
    r.created_at = obj["created_at"].toString().toStdString();
    return r;
}

MyRecipeStatus parseMyRecipeStatus(const QJsonObject& obj) {
    MyRecipeStatus item;
    item.id           = obj["id"].toInt();
    item.name         = obj["name"].toString().toStdString();
    item.status       = obj["status"].toString().toStdString();
    if (obj.contains("reject_reason") && !obj["reject_reason"].isNull())
        item.reject_reason = obj["reject_reason"].toString().toStdString();
    item.submitted_at = obj["submitted_at"].toString().toStdString();
    item.updated_at   = obj["updated_at"].toString().toStdString();
    return item;
}

SubmitRecipeResponse parseSubmitRecipeResponse(const QJsonObject& obj) {
    SubmitRecipeResponse resp;
    resp.id     = obj["id"].toInt();
    resp.status = obj["status"].toString().toStdString();
    return resp;
}

NutritionReport parseNutritionReport(const QJsonObject& obj) {
    NutritionReport report;
    report.recipe_id   = obj["recipe_id"].toInt();
    report.recipe_name = obj["recipe_name"].toString().toStdString();

    const QJsonObject ps = obj["per_serving"].toObject();
    report.per_serving.calories     = ps["calories"].toDouble();
    report.per_serving.protein_g    = ps["protein_g"].toDouble();
    report.per_serving.fat_g        = ps["fat_g"].toDouble();
    report.per_serving.carbs_g      = ps["carbs_g"].toDouble();
    report.per_serving.fiber_g      = ps["fiber_g"].toDouble();
    report.per_serving.sodium_mg    = ps["sodium_mg"].toDouble();
    report.per_serving.vitamin_c_mg = ps["vitamin_c_mg"].toDouble();

    const QJsonArray breakdownArr = obj["ingredients_breakdown"].toArray();
    for (const QJsonValue& val : breakdownArr) {
        const QJsonObject item = val.toObject();
        NutritionBreakdownItem bi;
        bi.name      = item["name"].toString().toStdString();
        bi.calories  = item["calories"].toDouble();
        bi.protein_g = item["protein_g"].toDouble();
        bi.fat_g     = item["fat_g"].toDouble();
        bi.carbs_g   = item["carbs_g"].toDouble();
        report.ingredients_breakdown.push_back(std::move(bi));
    }

    const QJsonArray excludedArr = obj["excluded_ingredients"].toArray();
    for (const QJsonValue& val : excludedArr) {
        const QJsonObject item = val.toObject();
        ExcludedIngredient ei;
        ei.name   = item["name"].toString().toStdString();
        ei.reason = item["reason"].toString().toStdString();
        report.excluded_ingredients.push_back(std::move(ei));
    }

    report.health_notes = obj["health_notes"].toString().toStdString();
    if (obj.contains("has_data")) {
        // 新服务端：has_data 为权威判定（false=该菜谱暂无营养报告，展示空态）
        report.has_data = obj["has_data"].toBool();
    } else {
        // 旧服务端（v2.8-）无该字段：200 响应必有 per_serving 数据（无数据时返回 400），
        // 按 per_serving 存在性兜底；避免把"有数据的老响应"误判为空态
        report.has_data = obj.contains("per_serving") && obj["per_serving"].isObject();
    }
    return report;
}

// ======================== 用户 / 认证相关 ========================

LoginResponse parseLoginResponse(const QJsonObject& obj) {
    LoginResponse resp;
    resp.token    = obj["token"].toString().toStdString();
    resp.user_id  = obj["user_id"].toInt();
    resp.username = obj["username"].toString().toStdString();
    return resp;
}

UserProfile parseUserProfile(const QJsonObject& obj) {
    UserProfile profile;
    profile.id                   = obj["id"].toInt();
    profile.username             = obj["username"].toString().toStdString();
    profile.display_name         = obj["display_name"].toString().toStdString();
    profile.email                = obj["email"].toString().toStdString();
    profile.phone                = obj["phone"].toString().toStdString();
    profile.avatar_url           = obj["avatar_url"].toString().toStdString();
    profile.preferences_complete = obj["preferences_complete"].toBool();
    profile.created_at           = obj["created_at"].toString().toStdString();
    return profile;
}

UserPreferences parseUserPreferences(const QJsonObject& obj) {
    UserPreferences prefs;
    if (obj.contains("likes")) {
        for (const QJsonValue& v : obj["likes"].toArray())
            prefs.likes.push_back(v.toString().toStdString());
    }
    if (obj.contains("dislikes")) {
        for (const QJsonValue& v : obj["dislikes"].toArray())
            prefs.dislikes.push_back(v.toString().toStdString());
    }
    if (obj.contains("health_goal"))
        prefs.health_goal = obj["health_goal"].toString().toStdString();
    return prefs;
}

HealthProfileResponse parseHealthProfileResponse(const QJsonObject& obj) {
    HealthProfileResponse resp;
    if (obj.contains("height_cm"))
        resp.height_cm = obj["height_cm"].toInt();
    if (obj.contains("weight_kg"))
        resp.weight_kg = obj["weight_kg"].toDouble();
    if (obj.contains("conditions")) {
        for (const QJsonValue& v : obj["conditions"].toArray())
            resp.conditions.push_back(v.toString().toStdString());
    }
    if (obj.contains("suggested_avoidances")) {
        for (const QJsonValue& v : obj["suggested_avoidances"].toArray()) {
            const QJsonObject item = v.toObject();
            AvoidanceItem ai;
            ai.ingredient = item["ingredient"].toString().toStdString();
            ai.reason     = item["reason"].toString().toStdString();
            resp.suggested_avoidances.push_back(ai);
        }
    }
    return resp;
}

AvatarUploadResponse parseAvatarUploadResponse(const QJsonObject& obj) {
    AvatarUploadResponse resp;
    resp.avatar_url = obj["avatar_url"].toString().toStdString();
    return resp;
}

// ======================== 收藏 / 通知 / 公告 ========================

FavoriteItem parseFavoriteItem(const QJsonObject& obj) {
    FavoriteItem fi;
    fi.id           = obj["id"].toInt();
    fi.recipe_id    = obj["recipe_id"].toInt();
    fi.name         = obj["name"].toString().toStdString();
    fi.description  = obj["description"].toString().toStdString();
    fi.image_url    = obj["image_url"].toString().toStdString();
    fi.group_name   = obj["group_name"].toString().toStdString();
    fi.is_public    = obj["is_public"].toBool();
    fi.favorited_at = obj["favorited_at"].toString().toStdString();
    return fi;
}

FavoriteGroup parseFavoriteGroup(const QJsonObject& obj) {
    FavoriteGroup g;
    g.id         = obj["id"].toInt();
    g.name       = obj["name"].toString().toStdString();
    g.sort_order = obj["sort_order"].toInt();
    g.count      = obj["count"].toInt();
    return g;
}

NotificationItem parseNotificationItem(const QJsonObject& obj) {
    NotificationItem ni;
    ni.id      = obj["id"].toInt();
    ni.title   = obj["title"].toString().toStdString();
    ni.content = obj["content"].toString().toStdString();
    ni.type    = obj["type"].toString().toStdString();
    if (obj.contains("sub_type") && !obj["sub_type"].isNull())
        ni.sub_type = obj["sub_type"].toString().toStdString();
    ni.is_read = obj["is_read"].toBool();
    if (obj.contains("related_id") && !obj["related_id"].isNull())
        ni.related_id = obj["related_id"].toInt();
    if (obj.contains("trigger_user_name") && !obj["trigger_user_name"].isNull())
        ni.trigger_user_name = obj["trigger_user_name"].toString().toStdString();
    ni.created_at = obj["created_at"].toString().toStdString();
    return ni;
}

NotificationUnreadSummary parseNotificationUnreadSummary(const QJsonObject& obj) {
    NotificationUnreadSummary summary;
    summary.unread_review        = obj["review"].toInt();
    summary.unread_interaction   = obj["interaction"].toInt();
    summary.has_new_announcement = obj["system"].toBool();
    return summary;
}

AnnouncementItem parseAnnouncementItem(const QJsonObject& obj) {
    AnnouncementItem item;
    item.id         = obj["id"].toInt();
    item.title      = obj["title"].toString().toStdString();
    item.content    = obj["content"].toString().toStdString();
    item.created_at = obj["created_at"].toString().toStdString();
    return item;
}

UserRatingItem parseUserRatingItem(const QJsonObject& obj) {
    UserRatingItem item;
    item.rating_id   = obj["rating_id"].toInt();
    item.recipe_id   = obj["recipe_id"].toInt();
    item.recipe_name = obj["recipe_name"].toString().toStdString();
    item.rating      = obj["rating"].toInt();
    item.comment     = obj["comment"].toString().toStdString();
    item.created_at  = obj["created_at"].toString().toStdString();
    item.updated_at  = obj["updated_at"].toString().toStdString();
    return item;
}

// ======================== 库存 / 购物清单 ========================

InventoryItem parseInventoryItem(const QJsonObject& obj) {
    InventoryItem item;
    item.id              = obj["id"].toInt();
    item.ingredient_name = obj["ingredient_name"].toString().toStdString();
    item.quantity        = obj["quantity"].toDouble();
    item.unit            = obj["unit"].toString().toStdString();
    if (obj.contains("expiry_date") && !obj["expiry_date"].isNull())
        item.expiry_date = obj["expiry_date"].toString().toStdString();
    item.added_at = obj["added_at"].toString().toStdString();
    return item;
}

ShoppingListSummary parseShoppingListSummary(const QJsonObject& obj) {
    ShoppingListSummary summary;
    summary.id         = obj["id"].toInt();
    summary.name       = obj["name"].toString().toStdString();
    summary.item_count = obj["item_count"].toInt();
    summary.created_at = obj["created_at"].toString().toStdString();
    return summary;
}

ShoppingListItem parseShoppingListItem(const QJsonObject& obj) {
    ShoppingListItem item;
    item.id                 = obj["id"].toInt();
    item.ingredient_name    = obj["ingredient_name"].toString().toStdString();
    item.required_quantity  = obj["required_quantity"].toDouble();
    item.inventory_quantity = obj["inventory_quantity"].toDouble();
    item.to_buy_quantity    = obj["to_buy_quantity"].toDouble();
    item.unit               = obj["unit"].toString().toStdString();
    item.checked            = obj["checked"].toBool();
    return item;
}

ShoppingList parseShoppingList(const QJsonObject& obj) {
    ShoppingList list;
    list.id   = obj["id"].toInt();
    list.name = obj["name"].toString().toStdString();
    if (obj.contains("items") && obj["items"].isArray()) {
        for (const QJsonValue& val : obj["items"].toArray())
            list.items.push_back(parseShoppingListItem(val.toObject()));
    }
    return list;
}

BatchShoppingResponse parseBatchShoppingResponse(const QJsonObject& obj) {
    BatchShoppingResponse resp;
    resp.message = obj["message"].toString().toStdString();
    if (obj.contains("items") && obj["items"].isArray()) {
        for (const QJsonValue& val : obj["items"].toArray())
            resp.items.push_back(parseShoppingListItem(val.toObject()));
    }
    return resp;
}

// ======================== 分页组合 ========================

namespace {

/// 通用分页组合：parsePagination + 逐项基础 DTO 解析（data 缺失/类型不符 → 空列表，等价于迁移前）
template <typename T, typename ParseItemFn>
PagedResult<T> parsePaged(const QJsonObject& root, ParseItemFn parseItem) {
    PagedResult<T> result;
    result.pagination = JsonDeserializer::parsePagination(root);
    const QJsonArray dataArr = root["data"].toArray();
    for (const QJsonValue& val : dataArr)
        result.data.push_back(parseItem(val.toObject()));
    return result;
}

} // namespace

PagedRecipes parsePagedRecipes(const QJsonObject& root) {
    return parsePaged<RecipeSummary>(root, parseRecipeSummary);
}

PagedRatings parsePagedRatings(const QJsonObject& root) {
    return parsePaged<RecipeRating>(root, parseRecipeRating);
}

PagedMyRecipes parsePagedMyRecipes(const QJsonObject& root) {
    return parsePaged<MyRecipeStatus>(root, parseMyRecipeStatus);
}

PagedFavorites parsePagedFavorites(const QJsonObject& root) {
    return parsePaged<FavoriteItem>(root, parseFavoriteItem);
}

PagedNotifications parsePagedNotifications(const QJsonObject& root) {
    return parsePaged<NotificationItem>(root, parseNotificationItem);
}

PagedAnnouncements parsePagedAnnouncements(const QJsonObject& root) {
    return parsePaged<AnnouncementItem>(root, parseAnnouncementItem);
}

PagedInventory parsePagedInventory(const QJsonObject& root) {
    return parsePaged<InventoryItem>(root, parseInventoryItem);
}

PagedUserRatings parsePagedUserRatings(const QJsonObject& root) {
    // 分页缺省口径历史为 page=1 / size=20（与通用 parsePaged 的 0 缺省不同，逐字保留）
    PagedUserRatings result;
    const QJsonObject paginationObj = root["pagination"].toObject();
    result.pagination.page        = paginationObj["page"].toInt(1);
    result.pagination.size        = paginationObj["size"].toInt(20);
    result.pagination.total       = paginationObj["total"].toInt(0);
    result.pagination.total_pages = paginationObj["total_pages"].toInt(0);
    for (const QJsonValue& val : root["data"].toArray())
        result.data.push_back(parseUserRatingItem(val.toObject()));
    return result;
}

PagedRecommendedRecipes parsePagedRecommendedRecipes(const QJsonObject& root) {
    PagedRecommendedRecipes result;
    result.pagination            = parsePagination(root);
    result.health_filter_applied = root["health_filter_applied"].toBool(false);
    const QJsonArray dataArr     = root["data"].toArray();
    for (const QJsonValue& val : dataArr)
        result.data.push_back(parseRecommendedRecipe(val.toObject()));
    return result;
}

} // namespace JsonDeserializer
