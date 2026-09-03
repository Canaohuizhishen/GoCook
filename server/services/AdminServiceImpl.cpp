#include "AdminServiceImpl.h"

using namespace gocook::services;
using namespace gocook::models;

PagedUsers AdminServiceImpl::getUsers(int, int, const nlohmann::json&) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::createUser(const CreateUserRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::updateUser(int, const UpdateUserRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::setUserStatus(int, const SetUserStatusRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::deleteUser(int) {
    throw ServiceException("功能暂未实现", 501);
}

PagedPendingRecipes AdminServiceImpl::getPendingRecipes(int, int) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::approveRecipe(int) {
    throw ServiceException("功能暂未实现", 501);
}
void AdminServiceImpl::rejectRecipe(int, const RejectRecipeRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
BatchReviewResponse AdminServiceImpl::batchReviewRecipes(const BatchReviewRequest&) {
    throw ServiceException("功能暂未实现", 501);
}

void AdminServiceImpl::publishAnnouncement(const AnnouncementRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
NotificationResponse AdminServiceImpl::sendNotification(const NotificationRequest&) {
    throw ServiceException("功能暂未实现", 501);
}
