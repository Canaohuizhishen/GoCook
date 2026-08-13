#pragma once

#include <gocook/IRecipeRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 菜谱仓库的 PostgreSQL 实现，承载 IRecipeRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 RecipeServiceImpl 调用。
 */
class PgRecipeRepository : public gocook::repository::IRecipeRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgRecipeRepository(ConnectionPool& db) : db_(db) {}

    // 检查是否存在食材和步骤完全一致的已审核菜谱（内容查重）
    bool existsByContent(const nlohmann::json& ingredients,
                          const nlohmann::json& steps) override;

    // 查询公开菜谱列表（分页，可带筛选条件）
    gocook::models::PagedRecipes findPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) override;

    // 按关键词搜索菜谱（分页，可带筛选条件）
    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                               int page, int size,
                                               const nlohmann::json& filters) override;

    // 查询智能推荐菜谱（结合用户偏好与库存，分页）
    gocook::models::PagedRecommendedRecipes findRecommendedRecipes(int userId,
                                                                    int page,
                                                                    int size) override;

    // 按 ID 查询菜谱详情（userId 非 0 时附带当前用户收藏状态）
    gocook::models::RecipeDetail findById(int recipeId, int userId = 0) override;

    // 查询菜谱关联视频列表
    std::vector<gocook::models::RecipeVideo> findVideos(int recipeId) override;

    // 查询菜谱评分与评论（分页）
    gocook::models::PagedRatings findRatings(int recipeId, int page,
                                             int size) override;

    // 创建菜谱投稿，返回投稿响应（含 ID 与状态）
    gocook::models::SubmitRecipeResponse create(
        int userId, const gocook::models::SubmitRecipeRequest& data) override;

    // 查询当前用户的投稿列表（分页，可按状态筛选）
    gocook::models::PagedMyRecipes findMySubmittedRecipes(
        int userId, int page, int size, const std::string& status) override;

    // 更新未审核菜谱，返回更新后的状态
    std::string update(int userId, int recipeId,
                       const gocook::models::EditRecipeRequest& updates) override;

    // 切换菜谱收藏状态（可指定分组与可见性）
    void toggleFavorite(int userId, int recipeId,
                        std::optional<int> groupId,
                        std::optional<bool> isPublic) override;

    // 写入评分与评论
    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& req) override;

    // 更新评论
    void updateRating(int userId, int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& req) override;

    // 删除评论
    void deleteRating(int userId, int recipeId, int ratingId) override;

    // 查询当前用户对某菜谱的评分（未评分返回 nullopt）
    std::optional<gocook::models::RecipeRating> findMyRating(
        int userId, int recipeId) override;

    // 查询当前用户的所有评论列表（分页）
    gocook::models::PagedUserRatings findMyRatings(int userId, int page,
                                                   int size) override;

    // 查询菜谱独立营养报告
    gocook::models::NutritionReport findNutrition(int recipeId) override;

    // 更新菜谱封面图片，返回 image_url
    std::string updateRecipeImage(int recipeId, const std::string& imagePath) override;

    // 更新菜谱某一步骤的图片，返回 image_url
    std::string updateStepImage(int recipeId, int stepIndex, const std::string& imagePath) override;

    // 删除待审核菜谱（仅非 approved 状态可删），同时清理图片文件
    void deleteRecipe(int userId, int recipeId) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
