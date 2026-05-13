#include "AdminHandler.h"
#include <nlohmann/json.hpp>
#include "../common/ErrorHelper.h"

using json = nlohmann::json;
using namespace gocook::models;

static json toJson(const Pagination& pag) {
    return {{"page", pag.page}, {"size", pag.size}, {"total", pag.total}, {"total_pages", pag.total_pages}};
}

static json toJson(const AdminUser& user) {
    return {
        {"id", user.id},
        {"username", user.username},
        {"email", user.email},
        {"role", user.role},
        {"status", user.status},
        {"created_at", user.created_at}
    };
}

static json toJson(const PagedUsers& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& u : paged.data)
        resp["data"].push_back(toJson(u));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const PendingRecipeItem& item) {
    return {
        {"id", item.id},
        {"name", item.name},
        {"author_id", item.author_id},
        {"author_name", item.author_name},
        {"submitted_at", item.submitted_at},
        {"status", item.status}
    };
}

static json toJson(const PagedPendingRecipes& paged) {
    json resp;
    resp["data"] = json::array();
    for (const auto& r : paged.data)
        resp["data"].push_back(toJson(r));
    resp["pagination"] = toJson(paged.pagination);
    return resp;
}

static json toJson(const BatchReviewResponse& resp) {
    return {
        {"message", resp.message},
        {"success_count", resp.success_count},
        {"failed_ids", resp.failed_ids}
    };
}

static json toJson(const NotificationResponse& resp) {
    return {
        {"notification_id", resp.notification_id},
        {"status", resp.status},
        {"estimated_recipients", resp.estimated_recipients}
    };
}

AdminHandler::AdminHandler(gocook::services::IAdminService& service,
                           AuthMiddleware& auth)
    : service_(service), auth_(auth) {}

bool AdminHandler::requireRole(const httplib::Request& req, httplib::Response& res,
                                const std::vector<std::string>& allowedRoles) {
    auto info = auth_.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
        return false;
    }
    for (const auto& role : allowedRoles) {
        if (info.role == role) return true;
    }
    setErrorResponse(res, 403, "权限不足");
    return false;
}

void AdminHandler::getUsers(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin"})) return;
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        nlohmann::json filters;
        if (req.has_param("username")) filters["username"] = req.get_param_value("username");
        auto result = service_.getUsers(page, size, filters);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::createUser(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin"})) return;
    try {
        json reqJson = json::parse(req.body);
        CreateUserRequest request;
        request.username = reqJson.at("username");
        request.password = reqJson.at("password");
        request.email = reqJson.value("email", "");
        request.role = reqJson.value("role", "user");
        service_.createUser(request);
        res.status = 201;
        res.body = json{{"message", "用户已创建"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::updateUser(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin"})) return;
    try {
        int userId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        UpdateUserRequest updates;
        if (reqJson.contains("email")) updates.email = reqJson["email"];
        if (reqJson.contains("role")) updates.role = reqJson["role"];
        service_.updateUser(userId, updates);
        res.status = 200;
        res.body = json{{"message", "用户已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::setUserStatus(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin"})) return;
    try {
        int userId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        SetUserStatusRequest request;
        request.status = reqJson.at("status");
        service_.setUserStatus(userId, request);
        res.status = 200;
        res.body = json{{"message", "用户状态已更新"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::deleteUser(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin"})) return;
    try {
        int userId = std::stoi(req.matches[1]);
        service_.deleteUser(userId);
        res.status = 204;
        res.body.clear();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::getPendingRecipes(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        auto result = service_.getPendingRecipes(page, size);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::approveRecipe(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        service_.approveRecipe(recipeId);
        res.status = 200;
        res.body = json{{"message", "菜谱已审核通过"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::rejectRecipe(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        int recipeId = std::stoi(req.matches[1]);
        json reqJson = json::parse(req.body);
        RejectRecipeRequest request;
        request.reason = reqJson.at("reason");
        service_.rejectRecipe(recipeId, request);
        res.status = 200;
        res.body = json{{"message", "菜谱已拒绝"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::batchReviewRecipes(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        json reqJson = json::parse(req.body);
        BatchReviewRequest request;
        request.recipe_ids = reqJson.at("recipe_ids").get<std::vector<int>>();
        request.action = reqJson.at("action");
        if (reqJson.contains("reason")) request.reason = reqJson["reason"];
        auto result = service_.batchReviewRecipes(request);
        res.status = 200;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::publishAnnouncement(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        json reqJson = json::parse(req.body);
        AnnouncementRequest request;
        request.title = reqJson.at("title");
        request.content = reqJson.at("content");
        service_.publishAnnouncement(request);
        res.status = 201;
        res.body = json{{"message", "公告已发布"}}.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::sendNotification(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        json reqJson = json::parse(req.body);
        NotificationRequest request;
        request.title = reqJson.at("title");
        request.content = reqJson.at("content");
        request.target_type = reqJson.value("target_type", "all");
        if (reqJson.contains("target_ids")) request.target_ids = reqJson["target_ids"].get<std::vector<int>>();
        if (reqJson.contains("channels")) request.channels = reqJson["channels"].get<std::vector<std::string>>();
        if (reqJson.contains("scheduled_at")) request.scheduled_at = reqJson["scheduled_at"];
        auto result = service_.sendNotification(request);
        res.status = 201;
        res.body = toJson(result).dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::getStatistics(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        auto stats = service_.getStatistics();

        json resp;
        resp["total_users"] = stats.total_users;
        resp["active_users_7d"] = stats.active_users_7d;
        resp["total_recipes"] = stats.total_recipes;
        resp["pending_reviews"] = stats.pending_reviews;
        resp["new_recipes_week"] = stats.new_recipes_week;
        resp["new_comments_week"] = stats.new_comments_week;

        // growth 子对象
        json growthJson;
        growthJson["new_users_week"] = stats.growth.new_users_week;
        growthJson["new_users_week_growth"] = stats.growth.new_users_week_growth;
        growthJson["new_recipes_week_growth"] = stats.growth.new_recipes_week_growth;
        growthJson["active_users_7d_growth"] = stats.growth.active_users_7d_growth;
        resp["growth"] = growthJson;

        // top_recipes 数组
        json topRecipesJson = json::array();
        for (const auto& recipe : stats.top_recipes) {
            json recipeJson;
            recipeJson["id"] = recipe.id;
            recipeJson["name"] = recipe.name;
            recipeJson["view_count"] = recipe.view_count;
            topRecipesJson.push_back(recipeJson);
        }
        resp["top_recipes"] = topRecipesJson;

        // inventory_distribution 对象
        json inventoryDistJson;
        json categoriesJson = json::array();
        for (const auto& category : stats.inventory_distribution.categories) {
            json catJson;
            catJson["name"] = category.name;
            catJson["count"] = category.count;
            categoriesJson.push_back(catJson);
        }
        inventoryDistJson["categories"] = categoriesJson;
        resp["inventory_distribution"] = inventoryDistJson;

        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::getAdminLogs(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        std::string type = req.get_param_value("type");
        int userId = req.has_param("user_id") ? std::stoi(req.get_param_value("user_id")) : 0;
        auto result = service_.getAdminLogs(page, size, type, userId);
        // 简单序列化列表
        json resp;
        resp["data"] = json::array();
        for (const auto& log : result.data) {
            json item;
            item["id"] = log.id;
            item["operator_id"] = log.operator_id;
            item["operator_name"] = log.operator_name;
            item["type"] = log.type;
            item["action"] = log.action;
            item["target_id"] = log.target_id;
            item["target_name"] = log.target_name;
            item["detail"] = log.detail;
            item["result"] = log.result;
            item["created_at"] = log.created_at;
            resp["data"].push_back(item);
        }
        resp["pagination"]["page"] = result.pagination.page;
        resp["pagination"]["size"] = result.pagination.size;
        resp["pagination"]["total"] = result.pagination.total;
        resp["pagination"]["total_pages"] = result.pagination.total_pages;
        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}

void AdminHandler::getActivityLogs(const httplib::Request& req, httplib::Response& res) {
    if (!requireRole(req, res, {"super_admin", "moderator"})) return;
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 20;
        int userId = req.has_param("user_id") ? std::stoi(req.get_param_value("user_id")) : 0;
        std::string action = req.get_param_value("action");
        auto result = service_.getActivityLogs(page, size, userId, action);
        json resp;
        resp["data"] = json::array();
        for (const auto& log : result.data) {
            json item;
            item["id"] = log.id;
            item["user_id"] = log.user_id;
            item["username"] = log.username;
            item["action"] = log.action;
            item["target_type"] = log.target_type;
            item["target_id"] = log.target_id;
            item["detail"] = log.detail;
            item["created_at"] = log.created_at;
            resp["data"].push_back(item);
        }
        resp["pagination"]["page"] = result.pagination.page;
        resp["pagination"]["size"] = result.pagination.size;
        resp["pagination"]["total"] = result.pagination.total;
        resp["pagination"]["total_pages"] = result.pagination.total_pages;
        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}