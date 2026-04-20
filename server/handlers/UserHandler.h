#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>          // 依赖抽象 IUserService
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class UserHandler {
public:
    explicit UserHandler(gocook::services::IUserService& service);

    // 注册新用户（公开接口）
    void registerUser(const httplib::Request& req, httplib::Response& res);
    // 用户登录，返回 token（公开接口）
    void loginUser(const httplib::Request& req, httplib::Response& res);

    // 获取当前用户信息（需认证）
    void getCurrentUser(const httplib::Request& req, httplib::Response& res);
    // 更新当前用户个人资料（需认证）
    void updateProfile(const httplib::Request& req, httplib::Response& res);
    // 获取用户饮食偏好（需认证）
    void getPreferences(const httplib::Request& req, httplib::Response& res);
    // 更新用户饮食偏好（需认证）
    void updatePreferences(const httplib::Request& req, httplib::Response& res);
    // 录入/更新健康指标（需认证）
    void updateHealthProfile(const httplib::Request& req, httplib::Response& res);
    // 获取用户收藏列表（分页，需认证）
    void getFavorites(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IUserService& service_;   // 业务抽象，不接触数据库
};