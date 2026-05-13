#pragma once
#include <httplib/httplib.h>
#include <gocook/IServices.h>   // 只依赖抽象
#include "../auth_middleware.h"

class RecipeHandler
{
public:
    explicit RecipeHandler(gocook::services::IRecipeService& service,
                           AuthMiddleware& auth);

    // 公开菜谱列表
    void getRecipesPublic(const httplib::Request& req, httplib::Response& res);
    // 关键词搜索菜谱
    void searchRecipes(const httplib::Request& req, httplib::Response& res);
    // 智能推荐菜谱
    void getRecommendedRecipes(const httplib::Request& req, httplib::Response& res);
    // 获取菜谱详情
    void getRecipeDetail(const httplib::Request& req, httplib::Response& res);
    // 获取菜谱营养报告（v2.8）
    void getRecipeNutrition(const httplib::Request& req, httplib::Response& res);
    // 获取菜谱关联视频
    void getRecipeVideos(const httplib::Request& req, httplib::Response& res);
    // 获取菜谱评分与评论
    void getRecipeRatings(const httplib::Request& req, httplib::Response& res);
    // 修改评论
    void updateRating(const httplib::Request& req, httplib::Response& res);
    // 删除评论
    void deleteRating(const httplib::Request& req, httplib::Response& res);
    // 投稿新菜谱
    void submitRecipe(const httplib::Request& req, httplib::Response& res);
    // 获取我的投稿列表
    void getMySubmittedRecipes(const httplib::Request& req, httplib::Response& res);
    // 编辑未审核菜谱
    void editRecipe(const httplib::Request& req, httplib::Response& res);
    // 切换收藏状态
    void toggleFavorite(const httplib::Request& req, httplib::Response& res);
    // 评分与评论
    void rateRecipe(const httplib::Request& req, httplib::Response& res);

private:
    nlohmann::json parseFilterParams(const httplib::Request& req);
    gocook::services::IRecipeService& service_;
    AuthMiddleware& auth_;
};