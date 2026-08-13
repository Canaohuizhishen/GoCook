#pragma once
#include <gocook/IServices.h>
#include <gocook/IRecipeRepository.h>
#include <gocook/IUserRepository.h>
#include <gocook/IInventoryRepository.h>
#include <memory>

/**
 * @brief 菜谱服务实现，承载 IRecipeService 接口定义的全部菜谱业务逻辑。
 *
 * 组合菜谱、用户、库存三个仓库抽象完成数据访问与业务编排，
 * 由 RecipeHandler 调用，不直接接触数据库。
 */
class RecipeServiceImpl : public gocook::services::IRecipeService {
public:
    /**
     * @brief 构造函数，注入菜谱服务依赖的仓库抽象。
     * @param recipeRepo    菜谱仓库（必传）
     * @param userRepo      用户仓库（可选，用于作者信息等，默认 nullptr）
     * @param inventoryRepo 库存仓库（可选，用于推荐匹配，默认 nullptr）
     */
    explicit RecipeServiceImpl(
        std::unique_ptr<gocook::repository::IRecipeRepository> recipeRepo,
        std::unique_ptr<gocook::repository::IUserRepository> userRepo = nullptr,
        std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo = nullptr)
        : recipeRepo_(std::move(recipeRepo))
        , userRepo_(std::move(userRepo))
        , inventoryRepo_(std::move(inventoryRepo)) {}

    // 获取公开菜谱列表（分页，可带筛选条件）
    gocook::models::PagedRecipes getPublicRecipes(int page, int size,
                                                   const nlohmann::json& filters) override;

    // 按关键词搜索菜谱（分页，可带筛选条件）
    gocook::models::PagedRecipes searchRecipes(const std::string& keyword,
                                                int page, int size,
                                                const nlohmann::json& filters) override;

    // 结合用户偏好与库存生成智能推荐菜谱（分页）
    gocook::models::PagedRecommendedRecipes getRecommendedRecipes(int userId,
                                                                   int page, int size) override;

    // 获取菜谱详情（userId 非 0 时附带当前用户的收藏状态）
    gocook::models::RecipeDetail getRecipeDetail(int recipeId, int userId = 0) override;

    // 获取菜谱关联视频列表
    std::vector<gocook::models::RecipeVideo> getRecipeVideos(int recipeId) override;

    // 获取菜谱评分与评论（分页）
    gocook::models::PagedRatings getRecipeRatings(int recipeId, int page, int size) override;

    // 投稿新菜谱（入库前做内容查重）
    gocook::models::SubmitRecipeResponse submitRecipe(int userId,
                                                       const gocook::models::SubmitRecipeRequest& data) override;

    // 获取当前用户的投稿列表（分页，可按状态筛选）
    gocook::models::PagedMyRecipes getMySubmittedRecipes(int userId,
                                                          int page, int size,
                                                          const std::string& status = "") override;

    // 编辑未审核菜谱，返回状态（"pending"）
    std::string editRecipe(int userId, int recipeId,
                             const gocook::models::EditRecipeRequest& updates) override;

    // 切换菜谱收藏状态（可指定分组与可见性）
    void toggleFavorite(int userId, int recipeId,
                        std::optional<int> groupId = std::nullopt,
                        std::optional<bool> isPublic = std::nullopt) override;

    // 评分与评论
    void rateRecipe(int userId, int recipeId,
                    const gocook::models::RateRecipeRequest& request) override;

    // 修改评论
    void updateRating(int userId, int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& request) override;

    // 删除评论
    void deleteRating(int userId, int recipeId, int ratingId) override;

    // 获取当前用户对某菜谱的评分（未评分返回 nullopt）
    std::optional<gocook::models::RecipeRating> getMyRating(int userId,
                                                              int recipeId) override;

    // 获取当前用户的所有评论列表（分页）
    gocook::models::PagedUserRatings getMyRatings(int userId, int page, int size) override;

    // 获取独立营养报告
    gocook::models::NutritionReport getRecipeNutrition(int recipeId) override;

    // 上传菜谱封面图片，返回 image_url
    std::string uploadRecipeImage(int recipeId, const std::string& filePath) override;

    // 上传菜谱步骤图片，返回 image_url
    std::string uploadStepImage(int recipeId, int stepIndex, const std::string& filePath) override;

    // 删除待审核菜谱（仅非 approved 状态可删）
    void deleteRecipe(int userId, int recipeId) override;

private:
    std::unique_ptr<gocook::repository::IRecipeRepository> recipeRepo_;       ///< 菜谱仓库抽象
    std::unique_ptr<gocook::repository::IUserRepository> userRepo_;           ///< 用户仓库抽象（作者信息等）
    std::unique_ptr<gocook::repository::IInventoryRepository> inventoryRepo_; ///< 库存仓库抽象（推荐匹配用）
};
