#pragma once

#include <gocook/DataModels.h>

namespace gocook::repository {

/**
 * @brief 公告数据访问抽象接口，定义公告域的全部持久化操作。
 *
 * 由 server/repositories/PgAnnouncementRepository 实现（PostgreSQL），
 * 供 AnnouncementServiceImpl 依赖注入调用。
 */
class IAnnouncementRepository {
public:
    virtual ~IAnnouncementRepository() = default;

    /**
     * @brief 查询公告列表（分页）。
     * @param page 页码（从 1 开始）
     * @param size 每页数量
     * @return 分页的公告列表
     */
    virtual models::PagedAnnouncements findAll(int page, int size) = 0;
};

} // namespace gocook::repository
