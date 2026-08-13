#pragma once

#include <gocook/IServices.h>
#include <gocook/IAnnouncementRepository.h>
#include <memory>

/**
 * @brief 公告服务实现，承载 IAnnouncementService 接口定义的全部业务逻辑。
 *
 * 依赖公告仓库抽象完成数据访问，由 AnnouncementHandler 调用。
 */
class AnnouncementServiceImpl : public gocook::services::IAnnouncementService {
public:
    // 构造函数，注入公告仓库抽象
    explicit AnnouncementServiceImpl(std::unique_ptr<gocook::repository::IAnnouncementRepository> announcementRepo);

    /**
     * @brief 获取系统公告列表（分页）
     * @param page 页码
     * @param size 每页数量
     * @return 分页的公告列表
     * @throw gocook::services::ServiceException 当前版本始终抛出 "Not implemented"
     */
    gocook::models::PagedAnnouncements getAnnouncements(int page, int size) override;

private:
    std::unique_ptr<gocook::repository::IAnnouncementRepository> announcementRepo_; ///< 公告仓库抽象
};
