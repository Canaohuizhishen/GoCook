#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>          // 依赖抽象 IUserService
#include <nlohmann/json.hpp>
#include <string>
#include "../auth_middleware.h"

using json = nlohmann::json;

class UserHandler {
public:
    explicit UserHandler(gocook::services::IUserService& service,
                         AuthMiddleware& auth);

    // 注册新用户（公开接口）
    void registerUser(const httplib::Request& req, httplib::Response& res);
    // 用户登录，返回 token（公开接口）
    void loginUser(const httplib::Request& req, httplib::Response& res);
    // 忘记密码 - 发送重置邮件（公开接口）
    void forgotPassword(const httplib::Request& req, httplib::Response& res);
    // 重置密码（公开接口）
    void resetPassword(const httplib::Request& req, httplib::Response& res);

    // 获取当前用户信息（需认证）
    void getCurrentUser(const httplib::Request& req, httplib::Response& res);
    // 更新当前用户个人资料（需认证）
    void updateProfile(const httplib::Request& req, httplib::Response& res);
    // 头像上传（需认证）
    void uploadAvatar(const httplib::Request& req, httplib::Response& res);
    // 修改密码（需认证）
    void changePassword(const httplib::Request& req, httplib::Response& res);
    // 注销账户（需认证）
    void deleteAccount(const httplib::Request& req, httplib::Response& res);
    // 获取用户饮食偏好（需认证）
    void getPreferences(const httplib::Request& req, httplib::Response& res);
    // 更新用户饮食偏好（需认证）
    void updatePreferences(const httplib::Request& req, httplib::Response& res);
    // 录入/更新健康指标（需认证）
    void updateHealthProfile(const httplib::Request& req, httplib::Response& res);
    // 获取用户收藏列表（分页，需认证）
    void getFavorites(const httplib::Request& req, httplib::Response& res);

    // 收藏分组管理（需认证）
    void getFavoriteGroups(const httplib::Request& req, httplib::Response& res);
    void createFavoriteGroup(const httplib::Request& req, httplib::Response& res);
    void updateFavoriteGroup(const httplib::Request& req, httplib::Response& res);
    void deleteFavoriteGroup(const httplib::Request& req, httplib::Response& res);

    // 更新收藏项属性（移动分组/可见性，需认证）
    void updateFavoriteItem(const httplib::Request& req, httplib::Response& res);
    // 批量删除收藏（需认证）
    void batchDeleteFavorites(const httplib::Request& req, httplib::Response& res);

    // 通知中心（需认证）
    void getNotifications(const httplib::Request& req, httplib::Response& res);
    void markNotificationRead(const httplib::Request& req, httplib::Response& res);
    void markAllNotificationsRead(const httplib::Request& req, httplib::Response& res);
    void deleteNotification(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IUserService& service_;   // 业务抽象，不接触数据库
    AuthMiddleware& auth_;                     // 认证中间件
};