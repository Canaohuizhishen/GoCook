#pragma once

#include <gocook/IServices.h>
#include <gocook/IAnnouncementRepository.h>
#include <memory>

class AnnouncementServiceImpl : public gocook::services::IAnnouncementService {
public:
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
    std::unique_ptr<gocook::repository::IAnnouncementRepository> announcementRepo_;
};