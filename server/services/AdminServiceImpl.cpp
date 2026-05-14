#include "AdminServiceImpl.h"

using namespace gocook::services;
using namespace gocook::models;

PagedUsers AdminServiceImpl::getUsers(int, int, const nlohmann::json&) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::createUser(const CreateUserRequest&) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::updateUser(int, const UpdateUserRequest&) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::setUserStatus(int, const SetUserStatusRequest&) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::deleteUser(int) {
    throw ServiceException("Not implemented", 501);
}

PagedPendingRecipes AdminServiceImpl::getPendingRecipes(int, int) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::approveRecipe(int) {
    throw ServiceException("Not implemented", 501);
}
void AdminServiceImpl::rejectRecipe(int, const RejectRecipeRequest&) {
    throw ServiceException("Not implemented", 501);
}
BatchReviewResponse AdminServiceImpl::batchReviewRecipes(const BatchReviewRequest&) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::publishAnnouncement(const AnnouncementRequest&) {
    throw ServiceException("Not implemented", 501);
}
NotificationResponse AdminServiceImpl::sendNotification(const NotificationRequest&) {
    throw ServiceException("Not implemented", 501);
}
