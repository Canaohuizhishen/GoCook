#include "AdminHandler.h"
#include <gocook/IServices.h>

AdminHandler::AdminHandler(gocook::services::IAdminService& service) : service_(service) {}

// ---------- 用户管理 ----------
void AdminHandler::getUsers(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::createUser(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::updateUser(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::setUserStatus(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::deleteUser(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

// ---------- 菜谱审核 ----------
void AdminHandler::getPendingRecipes(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::approveRecipe(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::rejectRecipe(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::batchReviewRecipes(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

// ---------- 公告与通知 ----------
void AdminHandler::publishAnnouncement(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::sendNotification(const httplib::Request& req, httplib::Response& res) {
    throw gocook::services::ServiceException("Not implemented");
}