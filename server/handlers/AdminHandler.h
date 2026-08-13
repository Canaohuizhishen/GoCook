#pragma once

#include <vector>
#include <httplib/httplib.h>
#include <gocook/IServices.h>
#include "../middleware/auth_middleware.h"

/**
 * @brief 系统管理员处理器，负责处理后台管理相关的 HTTP 请求。
 *
 * 依赖 IAdminService 抽象接口，所有业务逻辑委托给服务层；
 * 受 AuthMiddleware 认证与角色权限校验保护。
 */
class AdminHandler {
public:
    // 构造函数，注入管理服务抽象与认证中间件
    explicit AdminHandler(gocook::services::IAdminService& service,
                          AuthMiddleware& auth);

    // 用户管理
    // 获取用户列表（分页，可带筛选条件）
    void getUsers(const httplib::Request& req, httplib::Response& res);
    // 创建用户账户
    void createUser(const httplib::Request& req, httplib::Response& res);
    // 修改用户信息
    void updateUser(const httplib::Request& req, httplib::Response& res);
    // 冻结/解封用户
    void setUserStatus(const httplib::Request& req, httplib::Response& res);
    // 删除用户
    void deleteUser(const httplib::Request& req, httplib::Response& res);

    // 菜谱审核
    // 获取待审核菜谱列表
    void getPendingRecipes(const httplib::Request& req, httplib::Response& res);
    // 审核通过菜谱
    void approveRecipe(const httplib::Request& req, httplib::Response& res);
    // 审核拒绝菜谱
    void rejectRecipe(const httplib::Request& req, httplib::Response& res);
    // 批量审核菜谱
    void batchReviewRecipes(const httplib::Request& req, httplib::Response& res);

    // 公告与通知
    // 发布系统公告
    void publishAnnouncement(const httplib::Request& req, httplib::Response& res);
    // 发送系统通知
    void sendNotification(const httplib::Request& req, httplib::Response& res);

    // 统计与日志
    // 获取运营数据统计
    void getStatistics(const httplib::Request& req, httplib::Response& res);
    // 获取管理员操作日志
    void getAdminLogs(const httplib::Request& req, httplib::Response& res);
    // 获取全局用户行为日志
    void getActivityLogs(const httplib::Request& req, httplib::Response& res);

private:
    // 校验当前用户角色是否在允许列表中，不满足时写入 403 响应并返回 false
    bool requireRole(const httplib::Request& req, httplib::Response& res,
                     const std::vector<std::string>& allowedRoles);
    gocook::services::IAdminService& service_; ///< 管理服务抽象
    AuthMiddleware& auth_;                     ///< 认证中间件
};
