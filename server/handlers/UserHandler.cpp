#include "UserHandler.h"

UserHandler::UserHandler(gocook::services::IUserService& service,
                         AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void UserHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    // 注册是公开接口，不需要 Token 验证，保持不变
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("username") || !reqJson.contains("password")) {
            res.status = 400;
            res.body = json{{"error", "Missing username or password"}}.dump();
            return;
        }
        gocook::models::RegisterRequest request;
        request.username = reqJson["username"];
        request.password = reqJson["password"];

        service_.registerUser(request);

        res.status = 201;
        res.body = json{{"message", "User registered successfully"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        std::string what = e.what();
        if (what.find("already exists") != std::string::npos) {
            res.status = 409;
        } else {
            res.status = 500;
        }
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void UserHandler::loginUser(const httplib::Request& req, httplib::Response& res) {
    // 登录是公开接口，不需要 Token 验证，保持不变
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("username") || !reqJson.contains("password")) {
            res.status = 400;
            res.body = json{{"error", "Missing username or password"}}.dump();
            return;
        }
        gocook::models::LoginRequest request;
        request.username = reqJson["username"];
        request.password = reqJson["password"];

        auto loginResp = service_.login(request);

        res.status = 200;
        res.body = json{
            {"token", loginResp.token},
            {"user_id", loginResp.user_id},
            {"username", loginResp.username}
        }.dump();
    } catch (const gocook::services::ServiceException& e) {
        res.status = 401;
        res.body = json{{"error", e.what()}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

// ---------- 以下方法需要认证，先验证 Token，再抛 Not implemented ----------

void UserHandler::getCurrentUser(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，调用 service_.getCurrentUser(userId)
    throw gocook::services::ServiceException("Not implemented");
}

void UserHandler::updateProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，解析请求体，调用 service_.updateProfile(...)
    throw gocook::services::ServiceException("Not implemented");
}

void UserHandler::getPreferences(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，调用 service_.getPreferences(userId)
    throw gocook::services::ServiceException("Not implemented");
}

void UserHandler::updatePreferences(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，解析请求体，调用 service_.updatePreferences(...)
    throw gocook::services::ServiceException("Not implemented");
}

void UserHandler::updateHealthProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，解析请求体，调用 service_.updateHealthProfile(...)
    throw gocook::services::ServiceException("Not implemented");
}

void UserHandler::getFavorites(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        res.status = 401;
        res.body = json{{"error", "Missing or invalid token"}}.dump();
        return;
    }
    // TODO: 实现 Token 验证，解析分页参数，调用 service_.getFavorites(userId, page, size)
    throw gocook::services::ServiceException("Not implemented");
}