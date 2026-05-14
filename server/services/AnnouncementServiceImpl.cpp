#include "AnnouncementServiceImpl.h"
#include <gocook/IServices.h>   // ServiceException 定义
#include <stdexcept>

using namespace gocook::services;
using namespace gocook::models;

// 构造函数：保存数据库连接池引用
AnnouncementServiceImpl::AnnouncementServiceImpl(ConnectionPool& db)
    : db_(db)
{
}

// 获取系统公告列表（当前为实现，抛出异常）
PagedAnnouncements AnnouncementServiceImpl::getAnnouncements(int /*page*/, int /*size*/)
{
    throw ServiceException("Not implemented", 501);
}