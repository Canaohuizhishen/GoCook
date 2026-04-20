#include "AnnouncementHandler.h"
#include <stdexcept>

AnnouncementHandler::AnnouncementHandler(gocook::services::IAnnouncementService& service)
    : service_(service)
{
}

void AnnouncementHandler::getAnnouncements(const httplib::Request& req, httplib::Response& res)
{
    // TODO: 解析分页参数、调用 service_.getAnnouncements() 并构造响应
    throw gocook::services::ServiceException("Not implemented");
}