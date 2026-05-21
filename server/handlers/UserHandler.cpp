#include "UserHandler.h"
#include <optional>
#include <iostream>
#include <fstream>
#include <chrono>
#include "../common/ErrorHelper.h"
#include "../common/PaginationHelper.h"
#include "../common/JsonSerializer.h"
#include "../common/Validation.h"
#include "../common/AuthHelper.h"
#include "../common/Logger.h"

using json = nlohmann::json;
using namespace gocook::models;

UserHandler::UserHandler(gocook::services::IUserService& service,
                         AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void UserHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        Validation::validateRegisterRequest(reqJson);

        RegisterRequest request;
        request.username = reqJson["username"];
        request.password = reqJson["password"];
        request.email = reqJson["email"];

        service_.registerUser(request);

        res.status = 201;
        res.body = json{{"message", "User registered successfully"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::loginUser(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        Validation::validateLoginRequest(reqJson);

        LoginRequest request;
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
        setErrorResponse(res, 401, "用户名或密码错误");
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::forgotPassword(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        Validation::validateForgotPasswordRequest(reqJson);

        std::string email = reqJson["email"];
        service_.requestPasswordReset(email);
        res.status = 200;
        res.body = json{{"message", "若该邮箱已注册，您将收到一封重置密码的邮件"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::resetPassword(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        Validation::validateResetPasswordRequest(reqJson);

        std::string token = reqJson["token"];
        std::string newPassword = reqJson["new_password"];
        service_.resetPassword(token, newPassword);
        res.status = 200;
        res.body = json{{"message", "密码重置成功"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        setErrorResponse(res, 400, "令牌无效或已过期");
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getCurrentUser(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto user = service_.getCurrentUser(info.userId);
        res.status = 200;
        res.body = JsonSerializer::toJson(user).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        UpdateProfileRequest profile;
        if (reqJson.contains("display_name")) profile.display_name = reqJson["display_name"].get<std::string>();
        if (reqJson.contains("avatar_url")) profile.avatar_url = reqJson["avatar_url"].get<std::string>();
        if (reqJson.contains("avatar_id")) profile.avatar_id = reqJson["avatar_id"].get<int>();
        if (reqJson.contains("email")) profile.email = reqJson["email"].get<std::string>();
        if (reqJson.contains("phone")) profile.phone = reqJson["phone"].get<std::string>();
        auto updated = service_.updateProfile(info.userId, profile);
        res.status = 200;
        res.body = JsonSerializer::toJson(updated).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

// 从 Content-Type 提取 MIME 类型（去掉 ;boundary 等参数）
static std::string extractMime(const std::string& ct) {
    auto p = ct.find(';');
    std::string m = (p == std::string::npos) ? ct : ct.substr(0, p);
    while (!m.empty() && (m.front()==' '||m.front()=='\t')) m.erase(0,1);
    while (!m.empty() && (m.back()==' '||m.back()=='\t')) m.pop_back();
    return m;
}

void UserHandler::uploadAvatar(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        // ======== 调试信息 ========
        std::cerr << "\n=== [AVATAR DEBUG] UserHandler::uploadAvatar ===" << std::endl;
        std::cerr << "[AVATAR] userId=" << info.userId << " username=" << info.username << std::endl;
        std::cerr << "[AVATAR] Content-Type raw: '" << req.get_header_value("Content-Type") << "'" << std::endl;
        std::cerr << "[AVATAR] body.size() = " << req.body.size() << " bytes" << std::endl;

        // 直接从 req.body 读取原始二进制数据（客户端直接发 POST body）
        const std::string& content = req.body;

        if (content.empty()) {
            std::cerr << "[AVATAR] ERROR: body is empty!" << std::endl;
            setErrorResponse(res, 400, "请选择 JPG 或 PNG 格式的图片");
            return;
        }

        // 从 Content-Type 头获取 MIME 类型
        std::string contentType = extractMime(req.get_header_value("Content-Type"));
        std::cerr << "[AVATAR] extracted MIME: '" << contentType << "'" << std::endl;

        // 校验文件类型
        if (contentType != "image/jpeg" && contentType != "image/png"
            && contentType != "image/jpg" && contentType != "image/gif"
            && contentType != "image/bmp" && contentType != "image/webp"
            && contentType != "image/svg+xml") {
            std::cerr << "[AVATAR] ERROR: unsupported MIME type!" << std::endl;
            setErrorResponse(res, 400, "不支持的图片格式，请使用 JPG/PNG/GIF/BMP/WEBP/SVG");
            return;
        }

        // 校验文件大小（不超过 5MB）
        if (content.size() > 5 * 1024 * 1024) {
            std::cerr << "[AVATAR] ERROR: file too large (" << content.size() << " bytes)" << std::endl;
            setErrorResponse(res, 400, "图片大小不能超过5MB");
            return;
        }

        // 写临时文件
        std::string ext = ".jpg";
        if (contentType == "image/png")          ext = ".png";
        else if (contentType == "image/gif")      ext = ".gif";
        else if (contentType == "image/bmp")      ext = ".bmp";
        else if (contentType == "image/webp")     ext = ".webp";
        else if (contentType == "image/svg+xml")  ext = ".svg";
        std::string tempPath = "/tmp/gocook_avatar_" + std::to_string(info.userId)
                             + "_" + std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()) + ext;
        std::cerr << "[AVATAR] tempPath = " << tempPath << std::endl;

        {
            std::ofstream ofs(tempPath, std::ios::binary);
            if (!ofs) {
                std::cerr << "[AVATAR] ERROR: failed to write temp file!" << std::endl;
                setErrorResponse(res, 500, "文件写入失败");
                return;
            }
            ofs.write(content.data(), content.size());
            ofs.close();
            std::cerr << "[AVATAR] temp file written OK (" << content.size() << " bytes)" << std::endl;
        }

        // 调用 Service 层
        std::cerr << "[AVATAR] calling service_.uploadAvatar(" << info.userId << ", " << tempPath << ")" << std::endl;
        auto result = service_.uploadAvatar(info.userId, tempPath);
        std::cerr << "[AVATAR] service returned: avatar_id=" << result.avatar_id
                  << " avatar_url='" << result.avatar_url << "'" << std::endl;

        res.status = 200;
        res.set_header("Content-Type", "application/json");
        res.body = json{
            {"avatar_id", result.avatar_id},
            {"avatar_url", result.avatar_url}
        }.dump();
        std::cerr << "[AVATAR] response body: " << res.body << std::endl;
        std::cerr << "=== [AVATAR END] ===" << std::endl;

    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::changePassword(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        Validation::validateChangePasswordRequest(reqJson);

        std::string current = reqJson["current_password"];
        std::string newPwd = reqJson["new_password"];
        service_.changePassword(info.userId, current, newPwd);
        res.status = 200;
        res.body = json{{"message", "密码修改成功"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::deleteAccount(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        service_.deleteAccount(info.userId);
        res.status = 200;
        res.body = json{{"message", "账户已注销"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getPreferences(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto prefs = service_.getPreferences(info.userId);
        res.status = 200;
        res.body = JsonSerializer::toJson(prefs).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updatePreferences(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        UserPreferences prefs;
        if (reqJson.contains("likes")) prefs.likes = reqJson["likes"].get<std::vector<std::string>>();
        if (reqJson.contains("dislikes")) prefs.dislikes = reqJson["dislikes"].get<std::vector<std::string>>();
        if (reqJson.contains("health_goal")) prefs.health_goal = reqJson["health_goal"].get<std::string>();
        service_.updatePreferences(info.userId, prefs);
        res.status = 200;
        res.body = json{{"message", "偏好已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateHealthProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        HealthProfileRequest health;
        if (reqJson.contains("height_cm")) health.height_cm = reqJson["height_cm"].get<int>();
        if (reqJson.contains("weight_kg")) health.weight_kg = reqJson["weight_kg"].get<double>();
        if (reqJson.contains("conditions")) health.conditions = reqJson["conditions"].get<std::vector<std::string>>();
        auto respData = service_.updateHealthProfile(info.userId, health);
        res.status = 200;
        res.body = JsonSerializer::toJson(respData).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getHealthProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto respData = service_.getHealthProfile(info.userId);
        res.status = 200;
        res.body = JsonSerializer::toJson(respData).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getFavorites(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto pp = parsePagination(req, 20);
        std::string group = req.get_param_value("group");
        auto result = service_.getFavorites(info.userId, pp.page, pp.size, group);
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getFavoriteGroups(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto groups = service_.getFavoriteGroups(info.userId);
        json arr = json::array();
        for (const auto& g : groups) arr.push_back(JsonSerializer::toJson(g));
        res.status = 200;
        res.body = arr.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::createFavoriteGroup(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        CreateGroupRequest request;
        request.name = reqJson.at("name");
        auto group = service_.createFavoriteGroup(info.userId, request);
        res.status = 201;
        res.body = JsonSerializer::toJson(group).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateFavoriteGroup(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int groupId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        UpdateGroupRequest request;
        request.name = reqJson.at("name");
        service_.updateFavoriteGroup(info.userId, groupId, request);
        res.status = 200;
        res.body = json{{"message", "分组已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::deleteFavoriteGroup(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int groupId = std::stoi(req.matches[1]);
        service_.deleteFavoriteGroup(info.userId, groupId);
        res.status = 200;
        res.body = json{{"message", "分组已删除"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateFavoriteItem(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int favoriteId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        UpdateFavoriteRequest request;
        if (reqJson.contains("group_id")) request.group_id = reqJson["group_id"].get<int>();
        if (reqJson.contains("is_public")) request.is_public = reqJson["is_public"].get<bool>();
        service_.updateFavoriteItem(info.userId, favoriteId, request);
        res.status = 200;
        res.body = json{{"message", "收藏项已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::batchDeleteFavorites(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        json reqJson = json::parse(req.body);
        BatchDeleteFavoritesRequest request;
        request.favorite_ids = reqJson.at("favorite_ids").get<std::vector<int>>();
        service_.batchDeleteFavorites(info.userId, request);
        res.status = 200;
        res.body = json{{"message", "批量删除成功"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getNotifications(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        auto pp = parsePagination(req, 20);
        std::string type = req.get_param_value("type");
        auto result = service_.getNotifications(info.userId, pp.page, pp.size, type);
        res.status = 200;
        res.body = JsonSerializer::toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::markNotificationRead(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int id = std::stoi(req.matches[1]);
        service_.markNotificationRead(info.userId, id);
        res.status = 200;
        res.body = json{{"message", "已标记为已读"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::markAllNotificationsRead(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        service_.markAllNotificationsRead(info.userId);
        res.status = 200;
        res.body = json{{"message", "全部已标记为已读"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::deleteNotification(const httplib::Request& req, httplib::Response& res) {
    auto info = requireAuth(auth_, req, res);
    if (!info.valid) return;
    try {
        int id = std::stoi(req.matches[1]);
        service_.deleteNotification(info.userId, id);
        res.status = 200;
        res.body = json{{"message", "通知已删除"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}


