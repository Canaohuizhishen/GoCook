#include "UserHandler.h"
#include <optional>
#include "../common/ErrorHelper.h"

using json = nlohmann::json;
using namespace gocook::models;

// ========== 辅助序列化 ==========

static json toJson(const UserProfile& user) {
    return {
        {"id", user.id},
        {"username", user.username},
        {"display_name", user.display_name},
        {"email", user.email},
        {"phone", user.phone},
        {"avatar_url", user.avatar_url},
        {"preferences_complete", user.preferences_complete},
        {"created_at", user.created_at}
    };
}

static json toJson(const UserPreferences& prefs) {
    return {
        {"likes", prefs.likes},
        {"dislikes", prefs.dislikes},
        {"health_goal", prefs.health_goal}
    };
}

static json toJson(const AvoidanceItem& item) {
    return {{"ingredient", item.ingredient}, {"reason", item.reason}};
}

static json toJson(const HealthProfileResponse& resp) {
    json arr = json::array();
    for (const auto& item : resp.suggested_avoidances)
        arr.push_back(toJson(item));
    return {{"suggested_avoidances", arr}};
}

static json toJson(const FavoriteItem& item) {
    return {
        {"id", item.id},
        {"name", item.name},
        {"description", item.description},
        {"image_url", item.image_url},
        {"group_name", item.group_name},
        {"is_public", item.is_public},
        {"favorited_at", item.favorited_at}
    };
}

static json toJson(const FavoriteGroup& group) {
    return {
        {"id", group.id},
        {"name", group.name},
        {"sort_order", group.sort_order},
        {"count", group.count}
    };
}

static json toJson(const Pagination& pag) {
    return {
        {"page", pag.page},
        {"size", pag.size},
        {"total", pag.total},
        {"total_pages", pag.total_pages}
    };
}

static json toJson(const PagedFavorites& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& f : paged.data)
        resp["data"].push_back(toJson(f));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const NotificationItem& item) {
    return {
        {"id", item.id},
        {"title", item.title},
        {"content", item.content},
        {"type", item.type},
        {"sub_type", item.sub_type},
        {"is_read", item.is_read},
        {"related_id", item.related_id},
        {"trigger_user_name", item.trigger_user_name},
        {"created_at", item.created_at}
    };
}

static json toJson(const PagedNotifications& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& n : paged.data)
        resp["data"].push_back(toJson(n));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const UserRatingItem& item) {
    return {
        {"rating_id", item.rating_id},
        {"recipe_id", item.recipe_id},
        {"recipe_name", item.recipe_name},
        {"rating", item.rating},
        {"comment", item.comment},
        {"created_at", item.created_at},
        {"updated_at", item.updated_at}
    };
}

static json toJson(const PagedUserRatings& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

UserHandler::UserHandler(gocook::services::IUserService& service,
                         AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

void UserHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("username") || !reqJson.contains("password") || !reqJson.contains("email")) {
            setErrorResponse(res, 400, "缺少必填字段");
            return;
        }
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
        if (!reqJson.contains("username") || !reqJson.contains("password")) {
            setErrorResponse(res, 400, "缺少用户名或密码");
            return;
        }
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

// 忘记密码 - 发送重置邮件（公开）
void UserHandler::forgotPassword(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("email")) {
            setErrorResponse(res, 400, "缺少邮箱字段");
            return;
        }
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

// 重置密码（公开）
void UserHandler::resetPassword(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("token") || !reqJson.contains("new_password")) {
            setErrorResponse(res, 400, "缺少令牌或新密码");
            return;
        }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        auto user = service_.getCurrentUser(info.userId);
        res.status = 200;
        res.body = toJson(user).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateProfile(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
        res.body = toJson(updated).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

// 头像上传（需认证）
void UserHandler::uploadAvatar(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        // 实际实现需要 multipart 解析，暂时返回未实现
        throw gocook::services::ServiceException("Not implemented", 501);
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

// 修改密码（需认证）
void UserHandler::changePassword(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("current_password") || !reqJson.contains("new_password")) {
            setErrorResponse(res, 400, "缺少当前密码或新密码");
            return;
        }
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

// 注销账户（需认证）
void UserHandler::deleteAccount(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        auto prefs = service_.getPreferences(info.userId);
        res.status = 200;
        res.body = toJson(prefs).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updatePreferences(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        json reqJson = json::parse(req.body);
        HealthProfileRequest health;
        if (reqJson.contains("height_cm")) health.height_cm = reqJson["height_cm"].get<int>();
        if (reqJson.contains("weight_kg")) health.weight_kg = reqJson["weight_kg"].get<double>();
        if (reqJson.contains("conditions")) health.conditions = reqJson["conditions"].get<std::vector<std::string>>();
        auto respData = service_.updateHealthProfile(info.userId, health);
        res.status = 200;
        res.body = toJson(respData).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::getFavorites(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        std::string group = req.get_param_value("group"); // may be empty
        auto result = service_.getFavorites(info.userId, page, size, group);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

// ========== 收藏分组管理 ==========

void UserHandler::getFavoriteGroups(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        auto groups = service_.getFavoriteGroups(info.userId);
        json arr = json::array();
        for (const auto& g : groups) arr.push_back(toJson(g));
        res.status = 200;
        res.body = arr.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::createFavoriteGroup(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        json reqJson = json::parse(req.body);
        CreateGroupRequest request;
        request.name = reqJson.at("name");
        auto group = service_.createFavoriteGroup(info.userId, request);
        res.status = 201;
        res.body = toJson(group).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::updateFavoriteGroup(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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

// ========== 通知中心 ==========

void UserHandler::getNotifications(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        std::string type = req.get_param_value("type"); // may be empty
        auto result = service_.getNotifications(info.userId, page, size, type);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void UserHandler::markNotificationRead(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
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

// 我的评论列表
void UserHandler::getMyRatings(const httplib::Request& req, httplib::Response& res) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return;
    }
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        // 目前 IUserService 没有直接提供 getMyRatings，需要走 IRecipeService，这里暂时抛出未实现
        throw gocook::services::ServiceException("Not implemented", 501);
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}