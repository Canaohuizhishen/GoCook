#include "AdminServiceImpl.h"

using namespace gocook::services;
using namespace gocook::models;

// ==================== 用户管理 ====================

PagedUsers AdminServiceImpl::getUsers(int /*page*/, int /*size*/,
                                      const nlohmann::json& /*filters*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::createUser(const CreateUserRequest& /*userData*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::updateUser(int /*userId*/,
                                  const UpdateUserRequest& /*updates*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::setUserStatus(int /*userId*/,
                                     const SetUserStatusRequest& /*request*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::deleteUser(int /*userId*/) {
    throw ServiceException("Not implemented", 501);
}

// ==================== 菜谱审核 ====================

PagedPendingRecipes AdminServiceImpl::getPendingRecipes(int /*page*/, int /*size*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::approveRecipe(int /*recipeId*/) {
    throw ServiceException("Not implemented", 501);
}

void AdminServiceImpl::rejectRecipe(int /*recipeId*/,
                                    const RejectRecipeRequest& /*request*/) {
    throw ServiceException("Not implemented", 501);
}

BatchReviewResponse AdminServiceImpl::batchReviewRecipes(
    const BatchReviewRequest& /*request*/) {
    throw ServiceException("Not implemented", 501);
}

// ==================== 公告与通知 ====================

void AdminServiceImpl::publishAnnouncement(const AnnouncementRequest& /*request*/) {
    throw ServiceException("Not implemented", 501);
}

NotificationResponse AdminServiceImpl::sendNotification(
    const NotificationRequest& /*notification*/) {
    throw ServiceException("Not implemented", 501);
}