#include "RecipeServiceImpl.h"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cctype>

using namespace gocook::models;
using namespace gocook::services;

// ============================================================
// 推荐引擎配置常量（可调参数）
// ============================================================
namespace {

    // 从仓库多取多少倍候选，供过滤/多样化后裁切
    constexpr int CANDIDATE_MULTIPLIER = 3;

    // 最终评分权重
    constexpr double WEIGHT_INVENTORY    = 0.50;
    constexpr double WEIGHT_PREFERENCE   = 0.25;
    constexpr double WEIGHT_POPULARITY   = 0.10;
    constexpr double WEIGHT_RECENCY      = 0.05;
    constexpr double WEIGHT_NUTRITION    = 0.10;

    // 偏好加分/减分（独立于其他维度）
    constexpr double BOOST_LIKE_FLAVOR     = 0.15;
    constexpr double BOOST_LIKE_TAG        = 0.10;
    constexpr double PENALTY_DISLIKE       = 0.50;  // 厌食惩罚足够强，能翻转排名
    constexpr double BOOST_HEALTH_GOAL     = 0.10;

    // 流行度加分
    constexpr double BOOST_RATING_HIGH     = 0.08;  // avg_rating >= 4.5
    constexpr double BOOST_RATING_MED      = 0.05;  // avg_rating >= 4.0

    // 新鲜度加分（投稿 7 天内）
    constexpr double BOOST_RECENT          = 0.05;
    constexpr int    RECENT_DAYS           = 7;

    // 多样化约束
    constexpr int MAX_PER_FLAVOR    = 2;
    constexpr int MAX_PER_METHOD    = 3;

    // ── 健康条件 → 禁忌食材映射 ──
    // 注意：假设数据库健康条件使用中文 locale
    const std::unordered_map<std::string, std::vector<std::string>> CONDITION_AVOIDANCES = {
        {"高血压", {"盐", "酱油", "豆瓣酱", "咸菜", "腊肉", "咸鱼", "腐乳", "榨菜"}},
        {"糖尿病", {"糖", "白糖", "冰糖", "蜂蜜", "甜面酱", "炼乳", "果酱"}},
        {"高血脂", {"肥肉", "猪油", "黄油", "奶油", "五花肉", "油炸", "猪板油"}},
        {"痛风",   {"海鲜", "动物内脏", "啤酒", "浓汤", "香菇", "虾", "蟹"}},
    };

    /// 将 conditions 数组映射为禁忌食材集合（小写）
    std::unordered_set<std::string> computeAvoidances(
        const std::vector<std::string>& conditions)
    {
        std::unordered_set<std::string> result;
        for (const auto& cond : conditions) {
            auto it = CONDITION_AVOIDANCES.find(cond);
            if (it != CONDITION_AVOIDANCES.end()) {
                for (const auto& ing : it->second) {
                    std::string lower;
                    lower.reserve(ing.size());
                    for (char c : ing) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    result.insert(std::move(lower));
                }
            }
        }
        return result;
    }

    /// 检查菜谱的食材中是否包含任何禁忌食材
    bool hasAvoidedIngredient(const RecommendedRecipe& rec,
                              const std::unordered_set<std::string>& avoidances)
    {
        if (avoidances.empty()) return false;
        for (const auto& ing : rec.match_status.available_ingredients) {
            std::string lower;
            for (char c : ing.name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (avoidances.count(lower)) return true;
        }
        for (const auto& ing : rec.match_status.missing_ingredients) {
            std::string lower;
            for (char c : ing.name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (avoidances.count(lower)) return true;
        }
        return false;
    }

    /// 菜谱食材名集合（小写）
    std::unordered_set<std::string> ingredientNames(const RecommendedRecipe& rec) {
        std::unordered_set<std::string> names;
        for (const auto& ing : rec.match_status.available_ingredients) {
            std::string lower;
            for (char c : ing.name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            names.insert(std::move(lower));
        }
        for (const auto& ing : rec.match_status.missing_ingredients) {
            std::string lower;
            for (char c : ing.name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            names.insert(std::move(lower));
        }
        return names;
    }

    /// 将 ISO 日期字符串转为 time_t
    time_t parseISODate(const std::string& iso) {
        std::tm tm = {};
        std::stringstream ss(iso);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            // 尝试带时间的格式
            ss.clear();
            ss.str(iso);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        }
        if (ss.fail()) return 0;
#ifdef _WIN32
        return _mkgmtime(&tm);
#else
        return timegm(&tm);
#endif
    }

    /// 判断 time_t 是否在最近 N 天内
    bool isRecent(time_t ts, int days) {
        if (ts == 0) return false;
        time_t now = std::time(nullptr);
        return (now - ts) <= days * 86400;
    }

    /// 检查 recipe flavor 或 tags 是否与喜好列表匹配
    bool matchesLikes(const RecommendedRecipe& rec,
                      const std::vector<std::string>& likes)
    {
        if (likes.empty()) return false;
        // flavor 匹配
        for (const auto& like : likes) {
            std::string lowerLike;
            for (char c : like) lowerLike += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            // 匹配 flavor
            std::string lowerFlavor;
            for (char c : rec.flavor) lowerFlavor += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!rec.flavor.empty() && lowerFlavor == lowerLike) return true;
            // 匹配 tags
            for (const auto& tag : rec.tags) {
                std::string lowerTag;
                for (char c : tag) lowerTag += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lowerTag == lowerLike) return true;
            }
        }
        return false;
    }

    /// 检查菜谱是否包含任何厌恶食材
    bool hasDislikedIngredient(const RecommendedRecipe& rec,
                               const std::vector<std::string>& dislikes)
    {
        if (dislikes.empty()) return false;
        auto names = ingredientNames(rec);
        for (const auto& d : dislikes) {
            std::string lower;
            for (char c : d) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (names.count(lower)) return true;
        }
        return false;
    }

    /// 营养适配评分 (0.0 或 0.10)
    double nutritionFitScore(const RecommendedRecipe& rec,
                             const std::string& healthGoal)
    {
        if (healthGoal.empty()) return 0.0;
        std::string goal;
        for (char c : healthGoal) goal += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (goal == "高蛋白" && rec.protein_g >= 15.0) return BOOST_HEALTH_GOAL;
        if (goal == "低卡"   && rec.calories < 300)    return BOOST_HEALTH_GOAL;
        if (goal == "减脂"   && rec.fat_g < 10.0)      return BOOST_HEALTH_GOAL;
        if (goal == "增肌"   && rec.protein_g >= 20.0 && rec.fat_g < 15.0) return BOOST_HEALTH_GOAL;
        return 0.0;
    }

} // anonymous namespace

PagedRecipes RecipeServiceImpl::getPublicRecipes(int page, int size,
                                                  const nlohmann::json& filters) {
    return recipeRepo_->findPublicRecipes(page, size, filters);
}

PagedRecipes RecipeServiceImpl::searchRecipes(const std::string& keyword,
                                                int page, int size,
                                                const nlohmann::json& filters) {
    return recipeRepo_->searchRecipes(keyword, page, size, filters);
}

// ═══════════════════════════════════════════════════════════════
// 智能推荐算法（三层：库存匹配 → 健康过滤 → 偏好加权+多样化）
// ═══════════════════════════════════════════════════════════════
PagedRecommendedRecipes RecipeServiceImpl::getRecommendedRecipes(int userId,
                                                                  int page,
                                                                  int size) {
    if (!inventoryRepo_ || !userRepo_) {
        throw ServiceException("推荐功能未完全配置", 501);
    }

    // ── 1. 库存非空校验 ──
    {
        auto invPage = inventoryRepo_->findInventory(userId, 1, 1);
        if (invPage.pagination.total == 0) {
            throw ServiceException("您的库存为空，请先添加食材", 400);
        }
    }

    // ── 2. 加载用户偏好 ──
    UserPreferences prefs;
    try { prefs = userRepo_->getPreferences(userId); }
    catch (...) { /* 未设置偏好 → 使用空默认值 */ }

    // ── 3. 加载健康档案 → 忌口集合 ──
    std::unordered_set<std::string> avoidances;
    bool hasHealthProfile = false;
    try {
        auto conditions = userRepo_->getHealthConditions(userId);
        if (!conditions.empty()) {
            hasHealthProfile = true;
            avoidances = computeAvoidances(conditions);
        }
    } catch (...) { /* 无健康档案 */ }

    // ── 4. 从仓库拉取候选（size × CANDIDATE_MULTIPLIER） ──
    int candidateSize = size * CANDIDATE_MULTIPLIER;
    auto raw = recipeRepo_->findRecommendedRecipes(userId, 1, candidateSize);

    // ── 5. 健康过滤 ──
    std::vector<RecommendedRecipe> candidates;
    candidates.reserve(raw.data.size());
    int excludedByHealth = 0;
    for (auto& rec : raw.data) {
        if (!avoidances.empty() && hasAvoidedIngredient(rec, avoidances)) {
            ++excludedByHealth;
            continue;
        }
        candidates.push_back(std::move(rec));
    }

    // ── 6. 多因子评分 ──
    for (auto& rec : candidates) {
        // 偏好维度（仅 likes/dislikes）
        double prefMod = 0.0;
        if (matchesLikes(rec, prefs.likes)) {
            prefMod += BOOST_LIKE_FLAVOR;
        }
        if (!prefs.dislikes.empty() && hasDislikedIngredient(rec, prefs.dislikes)) {
            prefMod -= PENALTY_DISLIKE;
        }

        // 流行度
        double popBoost = 0.0;
        if (rec.avg_rating >= 4.5)
            popBoost = BOOST_RATING_HIGH;
        else if (rec.avg_rating >= 4.0)
            popBoost = BOOST_RATING_MED;

        // 新鲜度
        double recencyBoost = isRecent(parseISODate(rec.submitted_at), RECENT_DAYS)
            ? BOOST_RECENT : 0.0;

        // 营养适配
        double nutritionBoost = nutritionFitScore(rec, prefs.health_goal);

        // 最终复合评分：各维度独立加权求和
        // prefMod ∈ [-0.5, +0.15]，乘积贡献 ≈ ±0.125
        double finalScore =
              rec.match_score * WEIGHT_INVENTORY
            + prefMod         * WEIGHT_PREFERENCE
            + popBoost        * WEIGHT_POPULARITY
            + recencyBoost    * WEIGHT_RECENCY
            + nutritionBoost  * WEIGHT_NUTRITION;

        // 钳位到 [0, 1]
        rec.match_score = std::round(std::max(0.0, std::min(1.0, finalScore)) * 100.0) / 100.0;
    }

    // ── 7. 按复合评分降序排列 ──
    std::sort(candidates.begin(), candidates.end(),
              [](const RecommendedRecipe& a, const RecommendedRecipe& b) {
                  return a.match_score > b.match_score;
              });

    // ── 8. 多样化重排序 ──
    // 策略：同 flavor 最多 2 道，同 cooking_method 最多 3 道
    std::vector<RecommendedRecipe> diverse;
    diverse.reserve(candidates.size());
    std::unordered_map<std::string, int> flavorCount;
    std::unordered_map<std::string, int> methodCount;
    std::vector<bool> taken(candidates.size(), false);

    for (size_t pass = 0; diverse.size() < size && pass < candidates.size(); ++pass) {
        for (size_t i = 0; i < candidates.size() && diverse.size() < size; ++i) {
            if (taken[i]) continue;
            const auto& rec = candidates[i];
            bool skip = false;
            std::string fv; for (char c : rec.flavor) fv += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string cm; for (char c : rec.cooking_method) cm += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            auto fcnt = flavorCount[fv];
            auto mcnt = methodCount[cm];

            if (fcnt >= MAX_PER_FLAVOR) skip = true;
            if (mcnt >= MAX_PER_METHOD && !skip) {
                // 仅当同一方法且同一风味也满时才跳过
                if (fcnt >= MAX_PER_FLAVOR) skip = true;
                else skip = false; // 方法满但风味未满，仍可接受
            }

            // pass >= 1 时放宽约束
            if (pass >= 1) {
                if (fcnt >= MAX_PER_FLAVOR + 1 && mcnt >= MAX_PER_METHOD + 1) skip = true;
                else skip = false;
            }

            if (skip) continue;

            taken[i] = true;
            flavorCount[fv]++;
            methodCount[cm]++;
            diverse.push_back(rec);
        }
    }

    // 若多样化后不足 size，从未选取的候选中补足
    for (size_t i = 0; i < candidates.size() && diverse.size() < size; ++i) {
        if (!taken[i]) diverse.push_back(candidates[i]);
    }

    // ── 9. 构建响应 ──
    PagedRecommendedRecipes result;
    result.data = std::move(diverse);
    result.pagination.page = page;
    result.pagination.size = size;
    result.pagination.total = raw.pagination.total;
    result.pagination.total_pages = result.pagination.total > 0
        ? (result.pagination.total + size - 1) / size
        : 0;
    result.health_filter_applied = hasHealthProfile && excludedByHealth > 0;

    return result;
}

RecipeDetail RecipeServiceImpl::getRecipeDetail(int recipeId, int userId) {
    return recipeRepo_->findById(recipeId, userId);

}

SubmitRecipeResponse RecipeServiceImpl::submitRecipe(int userId, const SubmitRecipeRequest& data) {
    return recipeRepo_->create(userId, data);
}
std::vector<RecipeVideo> RecipeServiceImpl::getRecipeVideos(int recipeId) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    return recipeRepo_->findVideos(recipeId);
}
PagedRatings RecipeServiceImpl::getRecipeRatings(int recipeId, int page, int size) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    return recipeRepo_->findRatings(recipeId, page, size);
}
PagedMyRecipes RecipeServiceImpl::getMySubmittedRecipes(int userId, int page, int size, const std::string& status) {
    return recipeRepo_->findMySubmittedRecipes(userId, page, size, status);
}
std::string RecipeServiceImpl::editRecipe(int userId, int recipeId, const EditRecipeRequest& updates) {
    return recipeRepo_->update(userId, recipeId, updates);
}
void RecipeServiceImpl::toggleFavorite(int userId, int recipeId, std::optional<int> groupId, std::optional<bool> isPublic) {
    // Verify recipe exists
    auto recipe = recipeRepo_->findById(recipeId);
    if (recipe.id == 0) {
        throw ServiceException("菜谱不存在", 404);
    }
    recipeRepo_->toggleFavorite(userId, recipeId, groupId, isPublic);
}
void RecipeServiceImpl::rateRecipe(int userId, int recipeId,
                                   const RateRecipeRequest& request) {
    recipeRepo_->findById(recipeId);  // 验证菜谱存在，不存在自动抛 404
    recipeRepo_->rateRecipe(userId, recipeId, request);
}
void RecipeServiceImpl::updateRating(int userId, int recipeId, int ratingId,
                                     const RateRecipeRequest& request) {
    recipeRepo_->updateRating(userId, recipeId, ratingId, request);
}
void RecipeServiceImpl::deleteRating(int userId, int recipeId, int ratingId) {
    recipeRepo_->deleteRating(userId, recipeId, ratingId);
}

std::optional<RecipeRating> RecipeServiceImpl::getMyRating(int userId, int recipeId) {
    return recipeRepo_->findMyRating(userId, recipeId);
}

PagedUserRatings RecipeServiceImpl::getMyRatings(int userId, int page, int size) {
    return recipeRepo_->findMyRatings(userId, page, size);
}
NutritionReport RecipeServiceImpl::getRecipeNutrition(int recipeId) {
    return recipeRepo_->findNutrition(recipeId);
}

std::string RecipeServiceImpl::uploadRecipeImage(int recipeId, const std::string& filePath) {
    return recipeRepo_->updateRecipeImage(recipeId, filePath);
}

std::string RecipeServiceImpl::uploadStepImage(int recipeId, int stepIndex, const std::string& filePath) {
    return recipeRepo_->updateStepImage(recipeId, stepIndex, filePath);
}

void RecipeServiceImpl::deleteRecipe(int userId, int recipeId) {
    recipeRepo_->deleteRecipe(userId, recipeId);
}
