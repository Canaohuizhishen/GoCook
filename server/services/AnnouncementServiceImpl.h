#pragma once

#include <gocook/IServices.h>
#include "../ConnectionPool.h"

/**
 * @brief 系统公告服务实现类
 *
 * 实现 IAnnouncementService 抽象接口，当前版本仅提供骨架。
 * 后续将在此类中实现数据库交互逻辑。
 */
class AnnouncementServiceImpl : public gocook::services::IAnnouncementService {
public:
    /**
     * @brief 构造函数，注入数据库连接
     * @param db 数据库连接池引用
     */
    explicit AnnouncementServiceImpl(ConnectionPool& db);

    /**
     * @brief 获取系统公告列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @return 分页的公告列表
     * @throw gocook::services::ServiceException 当前版本始终抛出 "Not implemented"
     */
    gocook::models::PagedAnnouncements getAnnouncements(int page, int size) override;

private:
    ConnectionPool& db_;  ///< 数据库连接池引用
};