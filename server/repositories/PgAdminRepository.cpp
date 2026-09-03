#include "PgAdminRepository.h"
#include <gocook/IServices.h>

using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

PagedUsers PgAdminRepository::findUsers(int, int, const nlohmann::json&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::createUser(const CreateUserRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::updateUser(int, const UpdateUserRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::setUserStatus(int, const SetUserStatusRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::deleteUser(int) {
    throw ServiceException("功能暂未实现", 501);
}

PagedPendingRecipes PgAdminRepository::findPendingRecipes(int, int) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::approveRecipe(int) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::rejectRecipe(int, const RejectRecipeRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

BatchReviewResponse PgAdminRepository::batchReviewRecipes(const BatchReviewRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void PgAdminRepository::publishAnnouncement(const AnnouncementRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

NotificationResponse PgAdminRepository::sendNotification(const NotificationRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

StatisticsData PgAdminRepository::findStatistics() {
    throw ServiceException("功能暂未实现", 501);
}

PagedAdminLogs PgAdminRepository::findAdminLogs(int, int, const std::string&, int) {
    throw ServiceException("功能暂未实现", 501);
}

PagedActivityLogs PgAdminRepository::findActivityLogs(int, int, int, const std::string&) {
    throw ServiceException("功能暂未实现", 501);
}
