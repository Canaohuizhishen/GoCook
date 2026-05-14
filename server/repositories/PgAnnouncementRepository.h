#pragma once

#include <gocook/IAnnouncementRepository.h>
#include "../ConnectionPool.h"

class PgAnnouncementRepository : public gocook::repository::IAnnouncementRepository {
public:
    explicit PgAnnouncementRepository(ConnectionPool& db) : db_(db) {}

    gocook::models::PagedAnnouncements findAll(int page, int size) override;

private:
    ConnectionPool& db_;
};
