#include "PgUserRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"

using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

std::optional<UserAuthInfo> PgUserRepository::findByUsername(const std::string& username) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result r = txn.exec_params(
            "SELECT id, username, password_hash, role FROM users WHERE username = $1",
            username);
        if (r.empty()) return std::nullopt;
        UserAuthInfo info;
        info.id = r[0]["id"].as<int>();
        info.username = r[0]["username"].c_str();
        info.passwordHash = r[0]["password_hash"].c_str();
        info.role = r[0]["role"].c_str();
        txn.commit();
        return info;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

bool PgUserRepository::existsByEmail(const std::string& email) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result r = txn.exec_params(
            "SELECT id FROM users WHERE email = $1", email);
        txn.commit();
        return !r.empty();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::createUser(const std::string& username,
                                  const std::string& passwordHash,
                                  const std::string& email) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3)",
            username, passwordHash, email);
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::optional<UserProfile> PgUserRepository::findById(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result r = txn.exec_params(
            "SELECT id, username, display_name, email, phone, avatar_url, "
            "preferences_complete, created_at FROM users WHERE id = $1", userId);
        if (r.empty()) return std::nullopt;
        UserProfile u;
        u.id = r[0]["id"].as<int>();
        u.username = r[0]["username"].c_str();
        u.display_name = r[0]["display_name"].as<std::string>("");
        u.email = r[0]["email"].as<std::string>("");
        u.phone = r[0]["phone"].as<std::string>("");
        u.avatar_url = r[0]["avatar_url"].as<std::string>("");
        u.preferences_complete = r[0]["preferences_complete"].as<bool>();
        u.created_at = r[0]["created_at"].as<std::string>("");
        txn.commit();
        return u;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::updateProfile(int, const UpdateProfileRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::changePassword(int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::deleteAccount(int) {
    throw ServiceException("Not implemented", 501);
}

AvatarUploadResponse PgUserRepository::uploadAvatar(int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}

UserPreferences PgUserRepository::getPreferences(int) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::updatePreferences(int, const UserPreferences&) {
    throw ServiceException("Not implemented", 501);
}

std::vector<std::string> PgUserRepository::getHealthConditions(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        auto row = txn.exec_params(
            "SELECT conditions FROM health_profiles WHERE user_id = $1",
            userId);
        if (row.empty() || row[0][0].is_null()) return {};
        auto j = nlohmann::json::parse(row[0][0].c_str());
        std::vector<std::string> conditions;
        for (const auto& c : j)
            conditions.push_back(c.get<std::string>());
        return conditions;
    } catch (const std::exception& e) {
        LOG_ERROR("getHealthConditions failed: %s", e.what());
        return {};
    }
}

HealthProfileResponse PgUserRepository::updateHealthProfile(int, const HealthProfileRequest&) {
    throw ServiceException("Not implemented", 501);
}

PagedFavorites PgUserRepository::getFavorites(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}

std::vector<FavoriteGroup> PgUserRepository::getFavoriteGroups(int) {
    throw ServiceException("Not implemented", 501);
}

FavoriteGroup PgUserRepository::createFavoriteGroup(int, const CreateGroupRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::updateFavoriteGroup(int, int, const UpdateGroupRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::deleteFavoriteGroup(int, int) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::updateFavoriteItem(int, int, const UpdateFavoriteRequest&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::batchDeleteFavorites(int, const BatchDeleteFavoritesRequest&) {
    throw ServiceException("Not implemented", 501);
}

PagedNotifications PgUserRepository::getNotifications(int, int, int, const std::string&) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::markNotificationRead(int, int) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::markAllNotificationsRead(int) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::deleteNotification(int, int) {
    throw ServiceException("Not implemented", 501);
}
