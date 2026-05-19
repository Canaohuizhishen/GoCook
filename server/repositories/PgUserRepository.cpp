#include "PgUserRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cctype>

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

void PgUserRepository::updateProfile(int userId, const UpdateProfileRequest& profile) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE users SET "
            "display_name = COALESCE($1, display_name), "
            "email = COALESCE($2, email), "
            "phone = COALESCE($3, phone), "
            "avatar_url = COALESCE($4, avatar_url) "
            "WHERE id = $5",
            profile.display_name.has_value()
                ? profile.display_name.value().c_str()
                : nullptr,
            profile.email.has_value()
                ? profile.email.value().c_str()
                : nullptr,
            profile.phone.has_value()
                ? profile.phone.value().c_str()
                : nullptr,
            profile.avatar_url.has_value()
                ? profile.avatar_url.value().c_str()
                : nullptr,
            userId
        );
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::string PgUserRepository::getPasswordHash(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        pqxx::result r = txn.exec_params(
            "SELECT password_hash FROM users WHERE id = $1", userId);
        if (r.empty()) {
            throw ServiceException("用户不存在", 404);
        }
        std::string hash = r[0]["password_hash"].c_str();
        txn.commit();
        return hash;
    } catch (const gocook::services::ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getPasswordHash: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::changePassword(int userId, const std::string& newPasswordHash) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        auto r = txn.exec_params(
            "UPDATE users SET password_hash = $1 WHERE id = $2",
            newPasswordHash, userId);
        if (r.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }
        txn.commit();
    } catch (const gocook::services::ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in changePassword: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::deleteAccount(int) {
    throw ServiceException("Not implemented", 501);
}

AvatarUploadResponse PgUserRepository::uploadAvatar(int userId, const std::string& filePath) {
    try {
        std::cerr << "\n=== [AVATAR DEBUG] PgUserRepository::uploadAvatar ===" << std::endl;
        std::cerr << "[AVATAR-REPO] userId=" << userId << " filePath='" << filePath << "'" << std::endl;

        // Determine file extension from the uploaded file
        std::string ext = ".jpg";  // default
        auto dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string origExt = filePath.substr(dotPos);
            // Normalize to lowercase for comparison
            std::string lower;
            for (char c : origExt) lower += std::tolower(static_cast<unsigned char>(c));
            if (lower == ".png")      ext = ".png";
            else if (lower == ".gif") ext = ".gif";
            else if (lower == ".bmp") ext = ".bmp";
            else if (lower == ".webp") ext = ".webp";
            else if (lower == ".svg")  ext = ".svg";
        }
        std::cerr << "[AVATAR-REPO] detected ext='" << ext << "'" << std::endl;

        // Generate unique filename: user_<id>_<timestamp><ext>
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      now.time_since_epoch()).count();
        std::string filename = "user_" + std::to_string(userId)
                             + "_" + std::to_string(ts) + ext;
        std::cerr << "[AVATAR-REPO] filename='" << filename << "'" << std::endl;

        // Create uploads directory if needed
        std::string uploadDir = "uploads/avatars/";
        std::cerr << "[AVATAR-REPO] creating dir '" << uploadDir << "' (cwd matters)" << std::endl;
        std::filesystem::create_directories(uploadDir);
        std::string destPath = uploadDir + filename;

        // Check if temp source file exists
        bool srcExists = std::filesystem::exists(filePath);
        std::cerr << "[AVATAR-REPO] source file exists? " << (srcExists ? "YES" : "NO") << std::endl;
        if (!srcExists) {
            std::cerr << "[AVATAR-REPO] ERROR: source file does not exist!" << std::endl;
        }

        // Copy file to permanent location
        try {
            std::filesystem::copy(filePath, destPath,
                                  std::filesystem::copy_options::overwrite_existing);
            std::cerr << "[AVATAR-REPO] copied to '" << destPath << "'" << std::endl;
        } catch (const std::filesystem::filesystem_error& fe) {
            std::cerr << "[AVATAR-REPO] copy FAILED: " << fe.what() << std::endl;
            LOG_ERROR("Failed to copy avatar file from %s to %s: %s",
                      filePath.c_str(), destPath.c_str(), fe.what());
            throw ServiceException("头像文件保存失败");
        }

        // Build URL: for now use relative path; in production would be full URL
        std::string avatarUrl = "/" + destPath;
        std::cerr << "[AVATAR-REPO] avatarUrl = '" << avatarUrl << "'" << std::endl;

        // Update user's avatar_url in database
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        auto sqlResult = txn.exec_params(
            "UPDATE users SET avatar_url = $1 WHERE id = $2",
            avatarUrl, userId);
        std::cerr << "[AVATAR-REPO] SQL UPDATE affected " << sqlResult.affected_rows() << " rows" << std::endl;
        txn.commit();

        // Clean up temp file
        std::filesystem::remove(filePath);
        std::cerr << "[AVATAR-REPO] temp file removed" << std::endl;

        AvatarUploadResponse resp;
        resp.avatar_id = userId;   // Use userId as avatar resource identifier
        resp.avatar_url = avatarUrl;
        std::cerr << "[AVATAR-REPO] returning: avatar_id=" << resp.avatar_id
                  << " avatar_url='" << resp.avatar_url << "'" << std::endl;
        std::cerr << "=== [AVATAR-REPO END] ===" << std::endl;
        return resp;

    } catch (const std::exception& e) {
        std::cerr << "[AVATAR-REPO] EXCEPTION: " << e.what() << std::endl;
        LOG_ERROR("Database error in uploadAvatar: %s", e.what());
        throw ServiceException("头像上传失败");
    }
}

UserPreferences PgUserRepository::getPreferences(int) {
    throw ServiceException("Not implemented", 501);
}

void PgUserRepository::updatePreferences(int, const UserPreferences&) {
    throw ServiceException("Not implemented", 501);
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
