#pragma once

#include <gocook/DataModels.h>

namespace gocook::repository {

class IAnnouncementRepository {
public:
    virtual ~IAnnouncementRepository() = default;

    virtual models::PagedAnnouncements findAll(int page, int size) = 0;
};

} // namespace gocook::repository
