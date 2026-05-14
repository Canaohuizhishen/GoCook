#include "PgAnnouncementRepository.h"
#include <gocook/IServices.h>

using namespace gocook::repository;
using namespace gocook::services;
using namespace gocook::models;

PagedAnnouncements PgAnnouncementRepository::findAll(int, int) {
    throw ServiceException("Not implemented", 501);
}
