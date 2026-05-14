#include "DataMapper.h"

namespace DataMapper {

    // ---------- 基础模型 ----------

    QVariantMap toMap(const gocook::models::Pagination& pagination)
    {
        QVariantMap map;
        map["page"]        = pagination.page;
        map["size"]        = pagination.size;
        map["total"]       = pagination.total;
        map["total_pages"] = pagination.total_pages;
        return map;
    }

    QVariantMap toMap(const gocook::models::Nutrition& nutrition)
    {
        QVariantMap map;
        map["calories"] = nutrition.calories;
        map["protein"]  = nutrition.protein;
        map["fat"]      = nutrition.fat;
        map["carbs"]    = nutrition.carbs;
        return map;
    }

    QVariantMap toMap(const gocook::models::Ingredient& ingredient)
    {
        QVariantMap map;
        map["name"]     = QString::fromStdString(ingredient.name);
        map["quantity"] = ingredient.quantity;
        map["unit"]     = QString::fromStdString(ingredient.unit);
        return map;
    }

    QVariantMap toMap(const gocook::models::CookingStep& step)
    {
        QVariantMap map;
        map["order"]       = step.order;
        map["description"] = QString::fromStdString(step.description);
        if (step.duration.has_value())
            map["duration"] = step.duration.value();
        return map;
    }

    QVariantMap toMap(const gocook::models::AvoidanceItem& item)
    {
        QVariantMap map;
        map["ingredient"] = QString::fromStdString(item.ingredient);
        map["reason"]     = QString::fromStdString(item.reason);
        return map;
    }

    // MatchIngredient：与 Ingredient 字段类似但更简单（无 quantity 语义？直接使用 name/quantity/unit）
    QVariantMap toMap(const gocook::models::MatchIngredient& ingredient)
    {
        QVariantMap map;
        map["name"]     = QString::fromStdString(ingredient.name);
        map["quantity"] = ingredient.quantity;
        map["unit"]     = QString::fromStdString(ingredient.unit);
        return map;
    }

    // MissingIngredient：字段已修正为 quantity，对齐 API 契约
    QVariantMap toMap(const gocook::models::MissingIngredient& ingredient)
    {
        QVariantMap map;
        map["name"]     = QString::fromStdString(ingredient.name);
        map["quantity"] = ingredient.quantity;
        map["unit"]     = QString::fromStdString(ingredient.unit);
        return map;
    }

    // ---------- 菜谱相关 ----------

    QVariantMap toMap(const gocook::models::RecipeSummary& recipe)
    {
        QVariantMap item;
        item["id"]          = recipe.id;
        item["name"]        = QString::fromStdString(recipe.name);
        item["description"] = QString::fromStdString(recipe.description);
        item["imageUrl"]    = QString::fromStdString(recipe.image_url);
        item["prepTime"]    = recipe.prep_time_minutes;
        item["cookTime"]    = recipe.cook_time_minutes;
        // v2.7+ 新增的菜谱属性字段
        item["cookingMethod"]  = QString::fromStdString(recipe.cooking_method);
        item["flavor"]         = QString::fromStdString(recipe.flavor);
        item["ingredientType"] = QString::fromStdString(recipe.ingredient_type);
        item["calories"]       = recipe.calories;
        item["viewCount"]      = recipe.view_count;
        item["avgRating"]      = recipe.avg_rating;

        QStringList tagList;
        for (const auto& t : recipe.tags)
            tagList << QString::fromStdString(t);
        item["tags"] = tagList;

        item["authorId"]   = recipe.author_id;
        item["authorName"] = QString::fromStdString(recipe.author_name);

        return item;
    }

    QVariantMap toMap(const gocook::models::RecommendedRecipe& recipe)
    {
        // 继承 RecipeSummary 的通用字段
        QVariantMap item = toMap(static_cast<const gocook::models::RecipeSummary&>(recipe));

        item["matchScore"] = recipe.match_score;

        // match_status
        QVariantMap status;
        QVariantList available;
        for (const auto& ing : recipe.match_status.available_ingredients)
            available << toMap(ing);   // 现在 MatchIngredient 已有 toMap 重载
        status["available_ingredients"] = available;

        QVariantList missing;
        for (const auto& ing : recipe.match_status.missing_ingredients) {
            // MissingIngredient 结构稍有不同，使用对应的 toMap
            missing << toMap(ing);
        }
        status["missing_ingredients"] = missing;
        item["matchStatus"] = status;

        return item;
    }

    QVariantMap toMap(const gocook::models::RecipeDetail& detail)
    {
        QVariantMap item;
        item["id"]                = detail.id;
        item["name"]              = QString::fromStdString(detail.name);
        item["description"]       = QString::fromStdString(detail.description);
        item["imageUrl"]          = QString::fromStdString(detail.image_url);
        item["prepTime"]          = detail.prep_time_minutes;
        item["cookTime"]          = detail.cook_time_minutes;

        // 食材列表
        QVariantList ingredients;
        for (const auto& ing : detail.ingredients)
            ingredients << toMap(ing);
        item["ingredients"] = ingredients;

        // 烹饪步骤
        QVariantList steps;
        for (const auto& s : detail.steps)
            steps << toMap(s);
        item["steps"] = steps;

        // 营养信息
        item["nutrition"] = toMap(detail.nutrition);

        // 标签
        QStringList tagList;
        for (const auto& t : detail.tags)
            tagList << QString::fromStdString(t);
        item["tags"] = tagList;

        // 作者与时间
        item["authorId"]   = detail.author_id;
        item["authorName"] = QString::fromStdString(detail.author_name);
        item["createdAt"]  = QString::fromStdString(detail.created_at);

        return item;
    }

    QVariantMap toMap(const gocook::models::SubmitRecipeResponse& resp) {
        QVariantMap map;
        map["id"]     = resp.id;
        map["status"] = QString::fromStdString(resp.status);
        return map;
    }

    // ---------- 营养报告相关（v2.8 新增） ----------

    QVariantMap toMap(const gocook::models::NutritionBreakdownItem& item)
    {
        QVariantMap map;
        map["name"]      = QString::fromStdString(item.name);
        map["calories"]  = item.calories;
        map["protein_g"] = item.protein_g;
        map["fat_g"]     = item.fat_g;
        map["carbs_g"]   = item.carbs_g;
        return map;
    }

    QVariantMap toMap(const gocook::models::NutritionReport& report)
    {
        QVariantMap map;
        map["recipeId"]   = report.recipe_id;
        map["recipeName"] = QString::fromStdString(report.recipe_name);

        QVariantMap perServing;
        perServing["calories"]    = report.per_serving.calories;
        perServing["protein_g"]   = report.per_serving.protein_g;
        perServing["fat_g"]       = report.per_serving.fat_g;
        perServing["carbs_g"]     = report.per_serving.carbs_g;
        perServing["fiber_g"]     = report.per_serving.fiber_g;
        perServing["sodium_mg"]   = report.per_serving.sodium_mg;
        perServing["vitamin_c_mg"]= report.per_serving.vitamin_c_mg;
        map["per_serving"] = perServing;

        QVariantList breakdown;
        for (const auto& item : report.ingredients_breakdown)
            breakdown << toMap(item);
        map["ingredients_breakdown"] = breakdown;

        map["health_notes"] = QString::fromStdString(report.health_notes);
        return map;
    }

    // ---------- 用户 / 认证相关 ----------

    QVariantMap toMap(const gocook::models::FavoriteItem& item)
    {
        QVariantMap map;
        map["id"]           = item.id;
        map["name"]         = QString::fromStdString(item.name);
        map["description"]  = QString::fromStdString(item.description);
        map["imageUrl"]     = QString::fromStdString(item.image_url);
        map["favoritedAt"]  = QString::fromStdString(item.favorited_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::RecipeRating& rating)
    {
        QVariantMap map;
        map["id"]         = rating.id;
        map["userId"]     = rating.user_id;
        map["username"]   = QString::fromStdString(rating.username);
        map["rating"]     = rating.rating;
        map["comment"]    = QString::fromStdString(rating.comment);
        map["createdAt"]  = QString::fromStdString(rating.created_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::MyRecipeStatus& status)
    {
        QVariantMap map;
        map["id"]           = status.id;
        map["name"]         = QString::fromStdString(status.name);
        map["status"]       = QString::fromStdString(status.status);
        if (status.reject_reason.has_value())
            map["rejectReason"] = QString::fromStdString(status.reject_reason.value());
        map["submittedAt"]  = QString::fromStdString(status.submitted_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::UserProfile& user)
    {
        QVariantMap map;
        map["id"]        = user.id;
        map["username"]  = QString::fromStdString(user.username);
        map["email"]     = QString::fromStdString(user.email);
        map["phone"]     = QString::fromStdString(user.phone);
        map["avatarUrl"] = QString::fromStdString(user.avatar_url);
        map["createdAt"] = QString::fromStdString(user.created_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::LoginResponse& login)
    {
        QVariantMap map;
        map["token"]    = QString::fromStdString(login.token);
        map["userId"]   = login.user_id;
        map["username"] = QString::fromStdString(login.username);
        return map;
    }

    QVariantMap toMap(const gocook::models::HealthProfileResponse& health)
    {
        QVariantMap map;
        QVariantList avoidances;
        for (const auto& item : health.suggested_avoidances)
            avoidances << toMap(item);
        map["suggestedAvoidances"] = avoidances;
        return map;
    }

    // ---------- 通知 / 我的评论 ----------

    QVariantMap toMap(const gocook::models::NotificationItem& item)
    {
        QVariantMap map;
        map["id"]               = item.id;
        map["title"]            = QString::fromStdString(item.title);
        map["content"]          = QString::fromStdString(item.content);
        map["type"]             = QString::fromStdString(item.type);
        map["subType"]         = QString::fromStdString(item.sub_type);
        map["is_read"]          = item.is_read;
        map["relatedId"]       = item.related_id;
        map["triggerUserName"]= QString::fromStdString(item.trigger_user_name);
        map["createdAt"]        = QString::fromStdString(item.created_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::UserRatingItem& item)
    {
        QVariantMap map;
        map["ratingId"]   = item.rating_id;
        map["recipeId"]   = item.recipe_id;
        map["recipeName"] = QString::fromStdString(item.recipe_name);
        map["rating"]     = item.rating;
        map["comment"]    = QString::fromStdString(item.comment);
        map["createdAt"]  = QString::fromStdString(item.created_at);
        map["updatedAt"]  = QString::fromStdString(item.updated_at);
        return map;
    }

    // ---------- 库存 / 购物清单相关 ----------

    QVariantMap toMap(const gocook::models::InventoryItem& item)
    {
        QVariantMap map;
        map["id"]              = item.id;
        map["ingredientName"]  = QString::fromStdString(item.ingredient_name);
        map["quantity"]        = item.quantity;
        map["unit"]            = QString::fromStdString(item.unit);
        if (item.expiry_date.has_value())
            map["expiryDate"] = QString::fromStdString(item.expiry_date.value());
        map["addedAt"]         = QString::fromStdString(item.added_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::ShoppingListItem& item)
    {
        QVariantMap map;
        map["id"]                = item.id;
        map["ingredientName"]    = QString::fromStdString(item.ingredient_name);
        map["requiredQuantity"]  = item.required_quantity;
        map["inventoryQuantity"] = item.inventory_quantity;
        map["toBuyQuantity"]     = item.to_buy_quantity;
        map["unit"]              = QString::fromStdString(item.unit);
        map["checked"]           = item.checked;
        return map;
    }

    QVariantMap toMap(const gocook::models::ShoppingListSummary& summary)
    {
        QVariantMap map;
        map["id"]         = summary.id;
        map["name"]       = QString::fromStdString(summary.name);
        map["itemCount"]  = summary.item_count;
        map["createdAt"]  = QString::fromStdString(summary.created_at);
        return map;
    }

    QVariantMap toMap(const gocook::models::ShoppingList& list)
    {
        QVariantMap map;
        map["id"]   = list.id;
        map["name"] = QString::fromStdString(list.name);
        QVariantList items;
        for (const auto& item : list.items)
            items << toMap(item);
        map["items"] = items;
        return map;
    }

    // ---------- 膳食计划相关 ----------

    QVariantMap toMap(const gocook::models::MealPlanSummary& plan)
    {
        QVariantMap map;
        map["planId"]   = plan.plan_id;
        map["date"]     = QString::fromStdString(plan.date);
        map["mealType"] = QString::fromStdString(plan.meal_type);
        map["recipe"]   = toMap(plan.recipe);
        map["nutrition"] = toMap(plan.nutrition);
        return map;
    }

    QVariantMap toMap(const gocook::models::DailyMealDetails& daily)
    {
        QVariantMap map;
        if (daily.breakfast.has_value())
            map["breakfast"] = toMap(daily.breakfast.value());
        if (daily.lunch.has_value())
            map["lunch"] = toMap(daily.lunch.value());
        if (daily.dinner.has_value())
            map["dinner"] = toMap(daily.dinner.value());
        if (daily.snack.has_value())
            map["snack"] = toMap(daily.snack.value());
        return map;
    }

    QVariantMap toMap(const gocook::models::CalendarDay& day)
    {
        QVariantMap map;
        map["date"]        = QString::fromStdString(day.date);
        map["meals"]       = toMap(day.meals);
        map["dailyTotal"] = toMap(day.daily_total);
        return map;
    }

    QVariantMap toMap(const gocook::models::NutritionTrendItem& trend)
    {
        QVariantMap map;
        map["date"]        = QString::fromStdString(trend.date);
        map["actual"]      = toMap(trend.actual);
        map["recommended"] = toMap(trend.recommended);

        QVariantMap comparison;
        auto addComp = [&](const std::string& key, const gocook::models::NutritionComparisonItem& comp) {
            QVariantMap c;
            c["diff"]       = comp.diff;
            c["percentage"] = comp.percentage;
            comparison[QString::fromStdString(key)] = c;
        };
        addComp("calories", trend.comparison.calories);
        addComp("protein",  trend.comparison.protein);
        addComp("fat",      trend.comparison.fat);
        addComp("carbs",    trend.comparison.carbs);
        map["comparison"] = comparison;

        return map;
    }

    // ---------- 公告相关 ----------

    QVariantMap toMap(const gocook::models::AnnouncementItem& ann)
    {
        QVariantMap map;
        map["id"]        = ann.id;
        map["title"]     = QString::fromStdString(ann.title);
        map["content"]   = QString::fromStdString(ann.content);
        map["createdAt"] = QString::fromStdString(ann.created_at);
        return map;
    }

    // ---------- 分页结果（通用辅助模板，不再依赖 value_type） ----------
    namespace {
        // 泛型版本，使用 decltype 推导元素类型
        template<typename PagedType, typename Func>
        QVariantMap pagedToMap(const PagedType& paged, Func&& itemMapper) {
            QVariantMap result;
            QVariantList data;
            for (const auto& item : paged.data)
                data << itemMapper(item);
            result["data"]       = data;
            result["pagination"] = toMap(paged.pagination);
            return result;
        }
    }

    QVariantMap toMap(const gocook::models::PagedRecipes& paged) {
        return pagedToMap(paged, [](const gocook::models::RecipeSummary& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedRecommendedRecipes& paged) {
        return pagedToMap(paged, [](const gocook::models::RecommendedRecipe& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedFavorites& paged) {
        return pagedToMap(paged, [](const gocook::models::FavoriteItem& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedInventory& paged) {
        return pagedToMap(paged, [](const gocook::models::InventoryItem& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedRatings& paged) {
        return pagedToMap(paged, [](const gocook::models::RecipeRating& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedAnnouncements& paged) {
        return pagedToMap(paged, [](const gocook::models::AnnouncementItem& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedMyRecipes& paged) {
        return pagedToMap(paged, [](const gocook::models::MyRecipeStatus& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedUsers& paged) {
        return pagedToMap(paged, [](const gocook::models::AdminUser& u) -> QVariantMap {
            QVariantMap map;
            map["id"]         = u.id;
            map["username"]   = QString::fromStdString(u.username);
            map["email"]      = QString::fromStdString(u.email);
            map["role"]       = QString::fromStdString(u.role);
            map["status"]     = QString::fromStdString(u.status);
            map["createdAt"]  = QString::fromStdString(u.created_at);
            return map;
        });
    }
    QVariantMap toMap(const gocook::models::PagedMealPlans& paged) {
        return pagedToMap(paged, [](const gocook::models::MealPlanSummary& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedPendingRecipes& paged) {
        return pagedToMap(paged, [](const gocook::models::PendingRecipeItem& r) -> QVariantMap {
            QVariantMap map;
            map["id"]          = r.id;
            map["name"]        = QString::fromStdString(r.name);
            map["authorName"]  = QString::fromStdString(r.author_name);
            map["submittedAt"] = QString::fromStdString(r.submitted_at);
            map["status"]      = QString::fromStdString(r.status);
            return map;
        });
    }
    QVariantMap toMap(const gocook::models::PagedNotifications& paged) {
        return pagedToMap(paged, [](const gocook::models::NotificationItem& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedUserRatings& paged) {
        return pagedToMap(paged, [](const gocook::models::UserRatingItem& r) { return toMap(r); });
    }
    QVariantMap toMap(const gocook::models::PagedCalendarDays& paged) {
        return pagedToMap(paged, [](const gocook::models::CalendarDay& r) { return toMap(r); });
    }

} // namespace DataMapper