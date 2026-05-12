#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>
#include "../auth_middleware.h"

class AdminHandler {
public:
    explicit AdminHandler(gocook::services::IAdminService& service,
                          AuthMiddleware& auth);

    // 用户管理
    void getUsers(const httplib::Request& req, httplib::Response& res);
    void createUser(const httplib::Request& req, httplib::Response& res);
    void updateUser(const httplib::Request& req, httplib::Response& res);
    void setUserStatus(const httplib::Request& req, httplib::Response& res);
    void deleteUser(const httplib::Request& req, httplib::Response& res);

    // 菜谱审核
    void getPendingRecipes(const httplib::Request& req, httplib::Response& res);
    void approveRecipe(const httplib::Request& req, httplib::Response& res);
    void rejectRecipe(const httplib::Request& req, httplib::Response& res);
    void batchReviewRecipes(const httplib::Request& req, httplib::Response& res);

    // 公告与通知
    void publishAnnouncement(const httplib::Request& req, httplib::Response& res);
    void sendNotification(const httplib::Request& req, httplib::Response& res);

    // 统计与日志
    void getStatistics(const httplib::Request& req, httplib::Response& res);
    void getAdminLogs(const httplib::Request& req, httplib::Response& res);
    void getActivityLogs(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IAdminService& service_;
    AuthMiddleware& auth_;
};