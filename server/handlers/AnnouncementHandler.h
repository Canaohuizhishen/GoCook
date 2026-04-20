#pragma once

#include <httplib/httplib.h>
#include <gocook/IServices.h>  // 依赖抽象 IAnnouncementService

/**
 * @brief 系统公告处理器，负责处理公告相关的 HTTP 请求。
 *
 * 依赖 IAnnouncementService 抽象接口，所有业务逻辑委托给服务层。
 */
class AnnouncementHandler {
public:
    /**
     * @brief 构造函数，注入公告服务抽象。
     * @param service 公告服务接口引用
     */
    explicit AnnouncementHandler(gocook::services::IAnnouncementService& service);

    /**
     * @brief 获取系统公告列表（分页，无需认证）。
     * @param req HTTP 请求
     * @param res HTTP 响应
     */
    void getAnnouncements(const httplib::Request& req, httplib::Response& res);

private:
    gocook::services::IAnnouncementService& service_;  ///< 公告服务抽象引用
};