#pragma once

#include <gocook/IAnnouncementRepository.h>
#include "../common/ConnectionPool.h"

/**
 * @brief 公告仓库的 PostgreSQL 实现，承载 IAnnouncementRepository 接口定义的全部数据访问。
 *
 * 通过 ConnectionPool 借连接执行 SQL，由 AnnouncementServiceImpl 调用。
 */
class PgAnnouncementRepository : public gocook::repository::IAnnouncementRepository {
public:
    // 构造函数，注入数据库连接池引用
    explicit PgAnnouncementRepository(ConnectionPool& db) : db_(db) {}

    // 查询公告列表（分页）
    gocook::models::PagedAnnouncements findAll(int page, int size) override;

private:
    ConnectionPool& db_; ///< 数据库连接池引用
};
