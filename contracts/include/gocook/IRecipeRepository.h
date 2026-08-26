#pragma once

#include <gocook/DataModels.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>

namespace gocook::repository {

/**
 * @brief 菜谱数据访问抽象接口，定义菜谱域的全部持久化操作。
 *
 * 由 server/repositories/PgRecipeRepository 实现（PostgreSQL），
 * 供 RecipeServiceImpl 依赖注入调用。
 */
class IRecipeRepository {
public:
    virtual ~IRecipeRepository() = default;

    /**
     * @brief 查询公开菜谱列表（分页，可带筛选条件）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param filters 筛选条件（JSON 对象，如技法/口味/食材类型等）
     * @return 分页的菜谱摘要列表
     */
    virtual models::PagedRecipes findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) = 0;

    /**
     * @brief 按关键词搜索菜谱（分页，可带筛选条件）。
     * @param keyword 搜索关键词
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param filters 筛选条件（JSON 对象）
     * @return 分页的菜谱摘要列表
     */
    virtual models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) = 0;

    /**
     * @brief 查询智能推荐菜谱（结合用户偏好与库存）。
     *
     * 是推荐引擎在数据库层的核心查询，负责计算每个候选菜谱与用户库存的食材匹配度，
     * 并一次返回匹配分数、已有食材列表、缺失食材列表等结构化数据，
     * 供上层的 RecipeServiceImpl 进行后续的加权、过滤和多样化排序。
     *
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的推荐菜谱（含匹配度与库存匹配详情）
     */
    virtual models::PagedRecommendedRecipes findRecommendedRecipes(int userId,
                                                                    int page,
                                                                    int size) = 0;

    /**
     * @brief 按 ID 查询菜谱详情。
     * @param recipeId 菜谱 ID
     * @param userId 当前用户 ID，非 0 时附带其收藏状态
     * @return 菜谱详情
     */
    virtual models::RecipeDetail findById(int recipeId, int userId = 0) = 0;

    /**
     * @brief 查询菜谱关联视频列表。
     * @param recipeId 菜谱 ID
     * @return 视频列表
     */
    virtual std::vector<models::RecipeVideo> findVideos(int recipeId) = 0;

    /**
     * @brief 查询菜谱评分与评论（分页）。
     * @param recipeId 菜谱 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的评分评论列表
     */
    virtual models::PagedRatings findRatings(int recipeId, int page,
                                             int size) = 0;

    /// 检查是否存在食材和步骤完全一致的已审核菜谱（内容查重）
    virtual bool existsByContent(const nlohmann::json& ingredients,
                                  const nlohmann::json& steps) = 0;

    /**
     * @brief 创建菜谱投稿。
     * @param userId 投稿用户 ID
     * @param data 投稿数据（菜谱名/食材/步骤等）
     * @param nutritionInfo 服务层计算好的完整营养 JSON（flat 四项 + per_serving +
     *        ingredients_breakdown + health_notes）；无法计算时传空对象 "{}"
     * @return 投稿响应（含新菜谱 ID 与状态）
     */
    virtual models::SubmitRecipeResponse create(
        int userId, const models::SubmitRecipeRequest& data,
        const nlohmann::json& nutritionInfo) = 0;

    /**
     * @brief 查询当前用户的投稿列表（分页）。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @param status 状态筛选："pending"/"approved"/"rejected"，空串表示全部
     * @return 分页的投稿列表
     */
    virtual models::PagedMyRecipes findMySubmittedRecipes(
        int userId, int page, int size, const std::string& status) = 0;

    /**
     * @brief 更新未审核菜谱。
     * @param userId 操作者用户 ID
     * @param recipeId 菜谱 ID
     * @param updates 待更新的字段
     * @param nutritionInfo 服务层按新食材清单计算好的营养 JSON；无法计算且无手填时传
     *        nullopt（实现应保留库中已有 nutrition_info，编辑不因计算不可用而清空营养）
     * @return 更新后的状态（"pending"）
     */
    virtual std::string update(int userId, int recipeId,
                                const models::EditRecipeRequest& updates,
                                const std::optional<nlohmann::json>& nutritionInfo) = 0;

    /**
     * @brief 切换菜谱收藏状态（未收藏则收藏，已收藏则取消）。
     * @param userId 用户 ID
     * @param recipeId 菜谱 ID
     * @param groupId 可选目标分组 ID，不传使用默认分组
     * @param isPublic 可选可见性，nullopt 表示保持默认
     */
    virtual void toggleFavorite(int userId, int recipeId,
                                std::optional<int> groupId,
                                std::optional<bool> isPublic) = 0;

    /**
     * @brief 写入评分与评论。
     * @param userId 评分用户 ID
     * @param recipeId 菜谱 ID
     * @param req 评分（1-5）与评论内容
     */
    virtual void rateRecipe(int userId, int recipeId,
                            const models::RateRecipeRequest& req) = 0;

    /**
     * @brief 修改评论。
     * @param userId 评论作者用户 ID
     * @param recipeId 菜谱 ID
     * @param ratingId 评论 ID
     * @param req 新的评分与评论内容
     */
    virtual void updateRating(int userId, int recipeId, int ratingId,
                              const models::RateRecipeRequest& req) = 0;

    /**
     * @brief 删除评论。
     * @param userId 评论作者用户 ID
     * @param recipeId 菜谱 ID
     * @param ratingId 评论 ID
     */
    virtual void deleteRating(int userId, int recipeId, int ratingId) = 0;

    /**
     * @brief 查询当前用户对某菜谱的评分。
     * @param userId 用户 ID
     * @param recipeId 菜谱 ID
     * @return 评分记录；未评分时返回 nullopt
     */
    virtual std::optional<models::RecipeRating> findMyRating(
        int userId, int recipeId) = 0;

    /**
     * @brief 查询当前用户的全部评论列表（分页）。
     * @param userId 用户 ID
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的评论列表
     */
    virtual models::PagedUserRatings findMyRatings(int userId, int page,
                                                   int size) = 0;

    /**
     * @brief 查询菜谱独立营养报告。
     * @param recipeId 菜谱 ID
     * @return 营养报告（每份营养/食材明细/健康提示）
     */
    virtual models::NutritionReport findNutrition(int recipeId) = 0;

    /// 更新菜谱封面图片：将 imagePath 文件复制到 uploads 目录，返回可访问的 image_url
    virtual std::string updateRecipeImage(int recipeId,
                                          const std::string& imagePath) = 0;

    /// 更新菜谱某一步骤的图片：读 steps JSONB → 改 [stepIndex].image_url → 写回，返回 image_url
    virtual std::string updateStepImage(int recipeId, int stepIndex,
                                        const std::string& imagePath) = 0;

    /// 删除待审核菜谱（仅 status != 'approved' 的菜谱可删除），同时清理图片文件
    virtual void deleteRecipe(int userId, int recipeId) = 0;
};

} // namespace gocook::repository
