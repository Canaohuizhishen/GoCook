#include "AnnouncementServiceImpl.h"
#include <gocook/IServices.h>

using namespace gocook::services;
using namespace gocook::models;

AnnouncementServiceImpl::AnnouncementServiceImpl(
    std::unique_ptr<gocook::repository::IAnnouncementRepository> announcementRepo)
    : announcementRepo_(std::move(announcementRepo))
{
}

PagedAnnouncements AnnouncementServiceImpl::getAnnouncements(int, int) {
    throw ServiceException("Not implemented", 501);
}
