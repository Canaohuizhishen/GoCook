#include "AdminHandler.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AdminHandler::AdminHandler(gocook::services::IAdminService& service,
                           AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

// ---------- 用户管理 ----------
void AdminHandler::getUsers(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::createUser(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::updateUser(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::setUserStatus(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::deleteUser(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

// ---------- 菜谱审核 ----------
void AdminHandler::getPendingRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::approveRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::rejectRecipe(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::batchReviewRecipes(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

// ---------- 公告与通知 ----------
void AdminHandler::publishAnnouncement(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}

void AdminHandler::sendNotification(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    throw gocook::services::ServiceException("Not implemented");
}