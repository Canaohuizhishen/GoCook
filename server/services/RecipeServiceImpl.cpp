#include "RecipeServiceImpl.h"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cctype>
#include "../common/Logger.h"

using namespace gocook::models;
using namespace gocook::services;

// ============================================================
// 推荐引擎配置常量（可调参数）
// ============================================================
namespace {

    // 从仓库多取多少倍候选，供过滤/多样化后裁切
    constexpr int CANDIDATE_MULTIPLIER = 3;

    // 最终评分权重
    constexpr double WEIGHT_INVENTORY    = 0.50;  // 库存（已有食材占菜谱总食材的比例）
    constexpr double WEIGHT_PREFERENCE   = 0.25;  // 偏好（喜欢/厌恶）
    constexpr double WEIGHT_POPULARITY   = 0.10;  // 流行度（平均评分）
    constexpr double WEIGHT_RECENCY      = 0.05;  // 新鲜度（近期投稿）
    constexpr double WEIGHT_NUTRITION    = 0.10;  // 营养适配（健康目标）

    // 偏好加分/减分
    constexpr double BOOST_LIKE_FLAVOR     = 0.15;  // 菜谱风味命中用户“喜欢”列表时加的分数
    constexpr double BOOST_LIKE_TAG        = 0.10;  // 菜谱标签命中用户“喜欢”列表时加的分数
    constexpr double PENALTY_DISLIKE       = 0.50;  // 菜谱含用户“厌恶”食材时的惩罚值，厌食惩罚足够强，能翻转排名

    // 流行度加分
    constexpr double BOOST_RATING_HIGH     = 0.08;  // 菜谱平均评分 >= 4.5时加的分数
    constexpr double BOOST_RATING_MED      = 0.05;  // 菜谱平均评分 >= 4.0时加的分数

    // 新鲜度加分
    constexpr double BOOST_RECENT          = 0.05;  // 投稿时间在 RECENT_DAYS 天内的菜谱加的分数
    constexpr int    RECENT_DAYS           = 7;     // 时间窗口

    // 营养适配加分
    constexpr double BOOST_HEALTH_GOAL     = 0.10;  // 当菜谱的营养指标（蛋白、热量、脂肪）与用户的健康目标（高蛋白/低卡/减脂/增肌）匹配时加的分数

    // 多样化约束
    constexpr int MAX_PER_FLAVOR    = 2;  // 同一风味（如“麻辣”）最多出现的菜谱数量
    constexpr int MAX_PER_METHOD    = 3;  // 同一烹饪方法（如“炒”）最多出现的菜谱数量

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

    /// 检查菜谱风味是否命中用户“喜欢”列表（命中 → BOOST_LIKE_FLAVOR）
    bool matchesLikeFlavor(const RecommendedRecipe& rec,
                           const std::vector<std::string>& likes)
    {
        if (likes.empty() || rec.flavor.empty()) return false;
        std::string lowerFlavor;
        for (char c : rec.flavor) lowerFlavor += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const auto& like : likes) {
            std::string lowerLike;
            for (char c : like) lowerLike += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowerFlavor == lowerLike) return true;
        }
        return false;
    }

    /// 检查菜谱标签是否命中用户“喜欢”列表（命中 → BOOST_LIKE_TAG）
    bool matchesLikeTag(const RecommendedRecipe& rec,
                        const std::vector<std::string>& likes)
    {
        if (likes.empty() || rec.tags.empty()) return false;
        for (const auto& like : likes) {
            std::string lowerLike;
            for (char c : like) lowerLike += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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

    // ────────────────────────────────────────────────────────────
    // 营养计算工具（投稿/编辑时按食材清单自动计算，见方案 A+C）
    // ────────────────────────────────────────────────────────────

    /// 四舍五入到 1 位小数（入库与展示一致，避免 0.30000000000000004）
    double round1(double v) {
        return std::round(v * 10.0) / 10.0;
    }

    /// 名称规范化：去首尾空白 + 去掉中文/英文括号括注（如 "鸡胸肉（去皮）" → "鸡胸肉"）
    std::string normalizeIngredientName(const std::string& raw) {
        std::string s = raw;
        // trim
        size_t b = s.find_first_not_of(" \t\r\n");
        size_t e = s.find_last_not_of(" \t\r\n");
        if (b == std::string::npos) return "";
        s = s.substr(b, e - b + 1);
        // 去括号括注（保留括号前内容；中文括号与英文括号都处理）
        for (const std::string_view open : {"（", "("}) {
            auto pos = s.find(open);
            if (pos != std::string::npos) {
                s = s.substr(0, pos);
            }
        }
        // 再去一次尾空白（"鸡胸肉（去皮）" 处理完可能留尾空格）
        e = s.find_last_not_of(" \t\r\n");
        s = (e == std::string::npos) ? "" : s.substr(0, e + 1);
        return s;
    }

    /// 把"数量+单位"换算成克。返回 nullopt 表示无法换算（跳过该食材，不参与计算）。
    /// 规则：质量单位直接换算；毫升/升按 1ml≈1g 近似（水基食材误差可接受）；
    ///       容器单位按固定容量近似；计数单位（个/只/根…）依赖食材表
    ///       default_portion_g（单个近似质量），没有单重或单位未知/数量非正 → 无法换算。
    ///       单位先做首尾空白 trim（"克 " 等带空白输入不再被静默跳过）。
    std::optional<double> gramsOf(double quantity, const std::string& unit,
                                  double defaultPortionG) {
        if (quantity <= 0.0) return std::nullopt;
        if (unit.empty()) return std::nullopt;

        const size_t ub = unit.find_first_not_of(" \t\r\n");
        const size_t ue = unit.find_last_not_of(" \t\r\n");
        const std::string u = (ub == std::string::npos) ? "" : unit.substr(ub, ue - ub + 1);
        if (u.empty()) return std::nullopt;

        // 质量单位
        if (u == "克" || u == "g" || u == "G") return quantity;
        if (u == "千克" || u == "公斤" || u == "kg" || u == "KG") return quantity * 1000.0;
        if (u == "斤") return quantity * 500.0;
        if (u == "两") return quantity * 50.0;
        if (u == "磅" || u == "lb" || u == "LB") return quantity * 453.592;

        // 体积单位（近似 1ml ≈ 1g）：毫升/升直接换算；固定容器容量近似（约数，水基食材误差可接受）
        if (u == "毫升" || u == "ml" || u == "mL" || u == "ML") return quantity;
        if (u == "升" || u == "L" || u == "l") return quantity * 1000.0;
        if (u == "杯") return quantity * 240.0;   // 1 杯 ≈ 240ml（标准量杯）
        if (u == "碗") return quantity * 200.0;   // 1 碗 ≈ 200ml（家常饭碗）
        if (u == "汤匙") return quantity * 15.0;  // 1 汤匙 ≈ 15ml
        if (u == "勺") return quantity * 10.0;    // 1 勺 ≈ 10ml（中式汤勺）
        if (u == "茶匙") return quantity * 5.0;   // 1 茶匙 ≈ 5ml

        // 计数单位：依赖食材单重
        static const std::unordered_set<std::string> countUnits = {
            "个", "只", "条", "根", "片", "块", "把", "瓣", "颗", "枚", "粒", "份", "盒", "袋",
            "包", "瓶", "罐",
        };
        if (countUnits.count(u)) {
            if (defaultPortionG > 0.0) return quantity * defaultPortionG;
            return std::nullopt;  // 该食材没有单重 → 无法换算
        }
        return std::nullopt;  // 未知单位
    }

    /// 按阈值规则生成健康提示（措辞保守，无命中返回空串，前端自动隐藏）。
    /// 注意：per_serving 当前语义为整道菜营养合计（非单份），文案刻意不带"单份"字样。
    std::string buildHealthNotes(const gocook::models::NutritionReport::PerServing& ps) {
        std::string notes;
        auto add = [&](const std::string& text) {
            if (!notes.empty()) notes += " ";
            notes += text;
        };
        if (ps.sodium_mg >= 800.0)   add("钠含量较高，高血压患者建议减少额外用盐。");
        if (ps.calories >= 700.0)    add("热量偏高，建议适量食用。");
        if (ps.protein_g >= 25.0 && ps.fat_g < 15.0) add("高蛋白低脂，适合健身人群。");
        if (ps.fiber_g >= 5.0)       add("富含膳食纤维，有助于肠道健康。");
        return notes;
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
        if (matchesLikeFlavor(rec, prefs.likes)) {
            prefMod += BOOST_LIKE_FLAVOR;
        }
        if (matchesLikeTag(rec, prefs.likes)) {
            prefMod += BOOST_LIKE_TAG;
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
        // prefMod ∈ [-0.5, +0.25]（厌恶 -0.50 / 风味 +0.15 / 标签 +0.10，可叠加），乘积贡献 ∈ [-0.125, +0.0625]
        double finalScore =
              rec.match_score * WEIGHT_INVENTORY
            + prefMod         * WEIGHT_PREFERENCE
            + popBoost        * WEIGHT_POPULARITY
            + recencyBoost    * WEIGHT_RECENCY
            + nutritionBoost  * WEIGHT_NUTRITION;

        // 钳位成 [0, 1] 之间的两位小数
        rec.match_score = std::round(std::max(0.0, std::min(1.0, finalScore)) * 100.0) / 100.0;
    }

    // ── 7. 按复合评分降序排列 ──
    std::sort(candidates.begin(), candidates.end(),
              [](const RecommendedRecipe& a, const RecommendedRecipe& b) {
                  return a.match_score > b.match_score;
              });

    // ── 8. 多样化重排序 ──
    // 策略：同 flavor 最多 MAX_PER_FLAVOR 道，同 cooking_method 最多 MAX_PER_METHOD 道
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
    result.pagination.total = raw.pagination.total;  // total 保留原始候选数，过滤只影响 data 条数
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
    // 内容查重：食材清单和步骤完全一致则拒稿
    nlohmann::json ingredients = nlohmann::json::array();
    for (const auto& ing : data.ingredients) {
        ingredients.push_back({{"name", ing.name},
                               {"quantity", ing.quantity},
                               {"unit", ing.unit}});
    }
    nlohmann::json steps = nlohmann::json::array();
    for (const auto& step : data.steps) {
        nlohmann::json j;
        j["order"] = step.order;
        j["description"] = step.description;
        if (step.duration.has_value())
            j["duration"] = step.duration.value();
        steps.push_back(std::move(j));
    }

    if (recipeRepo_->existsByContent(ingredients, steps)) {
        throw ServiceException("食材与步骤与现有菜谱完全一致，疑似侵权", 409);
    }

    // 按食材清单自动计算营养；查询异常且无手填时得到 nullopt → 投稿按空对象处理
    auto nutritionInfo = buildNutritionInfo(data.ingredients, data.nutrition);

    return recipeRepo_->create(userId, data, nutritionInfo.value_or(nlohmann::json::object()));
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
    // 编辑会重算营养（按更新后的食材清单；无数据时得到空对象 {}）。
    // 营养表查询异常且无手填 → nullopt → 仓库保留库中已有 nutrition_info，
    // 编辑不因计算不可用而清空已保存的营养数据。
    auto nutritionInfo = buildNutritionInfo(updates.ingredients, updates.nutrition);
    return recipeRepo_->update(userId, recipeId, updates, nutritionInfo);
}
void RecipeServiceImpl::toggleFavorite(int userId, int recipeId, std::optional<int> groupId, std::optional<bool> isPublic) {
    // 验证菜谱存在
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

// ═══════════════════════════════════════════════════════════════
// 营养自动计算（方案 A+C）
// ═══════════════════════════════════════════════════════════════
namespace {

    /// 组装"回退形态"营养 JSON：有手填值 → flat 四项 + per_serving 四项（"用户可选补充"语义）；
    /// 无手填 → 空对象（前端显示"暂无营养报告"）。excluded 非空时附未计入明细。
    /// 未注入营养仓库 / 营养表查询异常 / 全部食材不可换算 三条回退路径统一走这里，
    /// 保证入库形态一致（旧 flat-only 形态不再产生）。
    nlohmann::json fallbackNutrition(
        const std::optional<Nutrition>& userNutrition,
        const std::vector<nlohmann::json>& excluded)
    {
        if (!userNutrition.has_value()) return nlohmann::json::object();
        nlohmann::json j;
        j["calories"] = userNutrition->calories;
        j["protein"]  = userNutrition->protein;
        j["fat"]      = userNutrition->fat;
        j["carbs"]    = userNutrition->carbs;
        j["per_serving"]["calories"]    = userNutrition->calories;
        j["per_serving"]["protein_g"]   = userNutrition->protein;
        j["per_serving"]["fat_g"]       = userNutrition->fat;
        j["per_serving"]["carbs_g"]     = userNutrition->carbs;
        j["per_serving"]["fiber_g"]     = 0.0;
        j["per_serving"]["sodium_mg"]   = 0.0;
        j["per_serving"]["vitamin_c_mg"]= 0.0;
        j["ingredients_breakdown"] = nlohmann::json::array();
        j["health_notes"] = "";
        if (!excluded.empty()) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& e : excluded) arr.push_back(e);
            j["excluded_ingredients"] = arr;
        }
        return j;
    }

} // anonymous namespace

std::optional<nlohmann::json> RecipeServiceImpl::buildNutritionInfo(
    const std::vector<Ingredient>& ingredients,
    const std::optional<Nutrition>& userNutrition)
{
    // 未注入营养仓库（测试/旧组装）→ 回退形态（手填四项或空对象）
    if (!nutritionRepo_) return fallbackNutrition(userNutrition, {});

    // ── 1. 名称规范化 + 批量查表（一次查询，保持顺序对齐）──
    std::vector<std::string> names;
    names.reserve(ingredients.size());
    for (const auto& ing : ingredients)
        names.push_back(normalizeIngredientName(ing.name));

    std::vector<std::optional<gocook::repository::IngredientNutrition>> rows;
    try {
        rows = nutritionRepo_->findByNames(names);
    } catch (const std::exception& e) {
        // 营养表缺失/数据库故障：投稿/编辑不应整体失败（记日志便于排查）。
        // 有手填值 → 回退使用；无手填 → 返回 nullopt：
        // 投稿按空对象处理，编辑由仓库保留库中已有 nutrition_info，不因计算不可用而清空。
        LOG_WARN("[NUTRITION] findByNames 查询异常，降级为回退营养: %s", e.what());
        if (userNutrition.has_value())
            return fallbackNutrition(userNutrition, {});
        return std::nullopt;
    }

    // ── 2. 逐食材换算克数 → 按每 100g 含量累加（breakdown 用四舍五入值，合计=明细可见和）；
    //        未收录/无法换算的食材收集进 excluded（不再静默丢弃）──
    double cal = 0.0, pro = 0.0, fat = 0.0, carb = 0.0, fib = 0.0, sod = 0.0, vitc = 0.0;
    std::vector<NutritionBreakdownItem> breakdown;
    std::vector<nlohmann::json> excluded;
    for (size_t i = 0; i < ingredients.size(); ++i) {
        if (i >= rows.size() || !rows[i]) {                    // 营养库未收录
            excluded.push_back({{"name", ingredients[i].name}, {"reason", "未收录"}});
            continue;
        }
        auto grams = gramsOf(ingredients[i].quantity, ingredients[i].unit,
                             rows[i]->default_portion_g);
        if (!grams.has_value()) {                              // 无法换算（计数单位无单重/未知单位）
            excluded.push_back({{"name", ingredients[i].name}, {"reason", "无法换算"}});
            continue;
        }
        const double factor = *grams / 100.0;

        NutritionBreakdownItem bi;
        bi.name = rows[i]->name;                             // 用规范名（别名命中时显示规范名）
        bi.calories  = round1(rows[i]->calories * factor);
        bi.protein_g = round1(rows[i]->protein_g * factor);
        bi.fat_g     = round1(rows[i]->fat_g * factor);
        bi.carbs_g   = round1(rows[i]->carbs_g * factor);
        cal  += bi.calories;
        pro  += bi.protein_g;
        fat  += bi.fat_g;
        carb += bi.carbs_g;
        fib  += round1(rows[i]->fiber_g * factor);
        sod  += round1(rows[i]->sodium_mg * factor);
        vitc += round1(rows[i]->vitamin_c_mg * factor);
        breakdown.push_back(std::move(bi));
    }
    if (!excluded.empty()) {
        LOG_WARN("[NUTRITION] %zu/%d 个食材未计入营养（未收录/无法换算）",
                 excluded.size(), static_cast<int>(ingredients.size()));
    }

    // 全部食材未收录/不可换算 → 无法自动计算：回退手填四项或空对象，并附未计入明细
    if (breakdown.empty()) return fallbackNutrition(userNutrition, excluded);

    // ── 3. 组装入库 JSON（flat 四项供列表/详情，rich 结构供报告/推荐，与 seed 数据同构）──
    NutritionReport::PerServing ps;
    ps.calories = round1(cal);
    ps.protein_g = round1(pro);
    ps.fat_g = round1(fat);
    ps.carbs_g = round1(carb);
    ps.fiber_g = round1(fib);
    ps.sodium_mg = round1(sod);
    ps.vitamin_c_mg = round1(vitc);

    nlohmann::json j;
    j["calories"] = ps.calories;
    j["protein"]  = ps.protein_g;
    j["fat"]      = ps.fat_g;
    j["carbs"]    = ps.carbs_g;
    j["per_serving"]["calories"]    = ps.calories;
    j["per_serving"]["protein_g"]   = ps.protein_g;
    j["per_serving"]["fat_g"]       = ps.fat_g;
    j["per_serving"]["carbs_g"]     = ps.carbs_g;
    j["per_serving"]["fiber_g"]     = ps.fiber_g;
    j["per_serving"]["sodium_mg"]   = ps.sodium_mg;
    j["per_serving"]["vitamin_c_mg"]= ps.vitamin_c_mg;

    nlohmann::json breakdownJson = nlohmann::json::array();
    for (const auto& bi : breakdown)
        breakdownJson.push_back({{"name", bi.name},
                                 {"calories", bi.calories},
                                 {"protein_g", bi.protein_g},
                                 {"fat_g", bi.fat_g},
                                 {"carbs_g", bi.carbs_g}});
    j["ingredients_breakdown"] = breakdownJson;
    j["excluded_ingredients"] = excluded;   // 恒为数组（无未计入项时为空数组）
    j["health_notes"] = buildHealthNotes(ps);

    LOG_DEBUG("[NUTRITION] recipe nutrition computed: %d ingredients, %zu matched, %zu excluded",
              static_cast<int>(ingredients.size()), breakdown.size(), excluded.size());
    return j;   // json → optional<json> 隐式转换
}
