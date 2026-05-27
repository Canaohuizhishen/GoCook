#include "PgUserRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include <filesystem>

// libpqxx 7.x deprecates exec_params(string_view, ...) in favor of exec(zview, ...).
// Silence the flood of warnings — cleanup tracked as separate issue.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cctype>

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

std::optional<UserAuthInfo> PgUserRepository::findByUsername(const std::string& username) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT id, username, password_hash, role FROM users WHERE username = $1 | $1=%s", username.c_str());
        pqxx::result r = txn.exec(
            "SELECT id, username, password_hash, role FROM users WHERE username = $1",
            pqxx::params{username});
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
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE email = $1 (existsByEmail) | $1=%s", email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE email = $1", pqxx::params{email});
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
        LOG_DEBUG("[SQL] INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3) | $1=%s $3=%s",
                 username.c_str(), email.c_str());
        txn.exec(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3)",
            pqxx::params{username, passwordHash, email});
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
        LOG_DEBUG("[SQL] SELECT ... FROM users WHERE id = $1 (findById) | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT id, username, display_name, email, phone, avatar_url, "
            "preferences_complete, created_at FROM users WHERE id = $1", pqxx::params{userId});
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

std::optional<int> PgUserRepository::findIdByEmail(const std::string& email) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE email = $1 (findIdByEmail) | $1=%s", email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE email = $1", pqxx::params{email});
        txn.commit();
        if (r.empty()) return std::nullopt;
        return r[0]["id"].as<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in findIdByEmail: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::optional<int> PgUserRepository::findIdByUsernameAndEmail(
    const std::string& username, const std::string& email)
{
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE username = $1 AND email = $2 | $1=%s $2=%s",
                 username.c_str(), email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE username = $1 AND email = $2",
            pqxx::params{username, email});
        txn.commit();
        if (r.empty()) return std::nullopt;
        return r[0]["id"].as<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in findIdByUsernameAndEmail: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::createPasswordResetToken(
    int userId, const std::string& token)
{
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] INSERT INTO password_reset_tokens (user_id, token, expires_at) VALUES ($1, $2, ...) | $1=%d", userId);
        txn.exec(
            "INSERT INTO password_reset_tokens (user_id, token, expires_at) "
            "VALUES ($1, $2, NOW() + INTERVAL '" + std::to_string(TOKEN_EXPIRY_MINUTES) + " minutes')",
            pqxx::params{userId, token});
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in createPasswordResetToken: %s", e.what());
        throw ServiceException("创建重置令牌失败");
    }
}

std::optional<int> PgUserRepository::findUserIdByResetToken(const std::string& token) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT user_id FROM password_reset_tokens WHERE token = $1 ...");
        pqxx::result r = txn.exec(
            "SELECT user_id FROM password_reset_tokens "
            "WHERE token = $1 AND used = false AND expires_at > NOW()",
            pqxx::params{token});
        txn.commit();
        if (r.empty()) return std::nullopt;
        return r[0]["user_id"].as<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in findUserIdByResetToken: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::markResetTokenUsed(const std::string& token) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE password_reset_tokens SET used = true WHERE token = $1");
        txn.exec(
            "UPDATE password_reset_tokens SET used = true WHERE token = $1",
            pqxx::params{token});
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in markResetTokenUsed: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::resetPasswordAndMarkTokenUsed(
    int userId, const std::string& newPasswordHash, const std::string& token)
{
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE users SET password_hash = $1 WHERE id = $2 | $2=%d", userId);
        auto r = txn.exec(
            "UPDATE users SET password_hash = $1 WHERE id = $2",
            pqxx::params{newPasswordHash, userId});
        if (r.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }
        LOG_DEBUG("[SQL] UPDATE password_reset_tokens SET used = true WHERE token = $1");
        txn.exec(
            "UPDATE password_reset_tokens SET used = true WHERE token = $1",
            pqxx::params{token});
        txn.commit();
        LOG_INFO("Password reset and token marked used for user %d", userId);
    } catch (const gocook::services::ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in resetPasswordAndMarkTokenUsed: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::updateProfile(int userId, const UpdateProfileRequest& profile) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE users SET display_name=COALESCE($1,...), ... WHERE id=$5 | $5=%d", userId);
        auto r = txn.exec(
            "UPDATE users SET "
            "display_name = COALESCE($1, display_name), "
            "email = COALESCE($2, email), "
            "phone = COALESCE($3, phone), "
            "avatar_url = COALESCE($4, avatar_url) "
            "WHERE id = $5",
            pqxx::params{profile.display_name.has_value()
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
            userId});
        if (r.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }
        txn.commit();
    } catch (const gocook::services::ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::vector<std::string> PgUserRepository::getHealthConditions(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT conditions FROM health_profiles WHERE user_id = $1 | $1=%d", userId);
        auto row = txn.exec(
            "SELECT conditions FROM health_profiles WHERE user_id = $1",
            pqxx::params{userId});
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

std::string PgUserRepository::getPasswordHash(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT password_hash FROM users WHERE id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT password_hash FROM users WHERE id = $1", pqxx::params{userId});
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
        LOG_DEBUG("[SQL] UPDATE users SET password_hash = $1 WHERE id = $2 | $2=%d", userId);
        auto r = txn.exec(
            "UPDATE users SET password_hash = $1 WHERE id = $2",
            pqxx::params{newPasswordHash, userId});
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

void PgUserRepository::deleteAccount(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // 1. Anonymize public content: recipes (author_id has no CASCADE)
        LOG_DEBUG("[SQL] UPDATE recipes SET author_id = NULL WHERE author_id = $1 | $1=%d", userId);
        txn.exec("UPDATE recipes SET author_id = NULL WHERE author_id = $1", pqxx::params{userId});
        //    ratings: ON DELETE CASCADE + NOT NULL → gets deleted with user, fine

        // 2. Delete user — CASCADE on FK constraints auto-clears:
        //    user_preferences, health_profiles, inventory, shopping_lists,
        //    shopping_list_items, notifications, meal_plans, favorites, favorite_groups
        LOG_DEBUG("[SQL] DELETE FROM users WHERE id = $1 | $1=%d", userId);
        auto delResult = txn.exec(
            "DELETE FROM users WHERE id = $1", pqxx::params{userId});
        if (delResult.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }

        txn.commit();
    } catch (const gocook::services::ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in deleteAccount: %s", e.what());
        throw ServiceException("账户注销失败");
    }
}

AvatarUploadResponse PgUserRepository::uploadAvatar(int userId, const std::string& filePath) {
    try {
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
            else if (lower == ".svg")  ext = ".svg";
        }

        // Generate unique filename: user_<id>_<timestamp><ext>
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      now.time_since_epoch()).count();
        std::string filename = "user_" + std::to_string(userId)
                             + "_" + std::to_string(ts) + ext;

        // Create uploads directory if needed (absolute path, 与 Router 文件服务路径一致)
        const char* envDir = std::getenv("GOCOOK_UPLOADS_DIR");
        std::string baseDir = envDir ? envDir : "server/uploads";
        std::string uploadDir = std::filesystem::absolute(baseDir + "/avatars/").string();
        std::filesystem::create_directories(uploadDir);
        std::string destPath = uploadDir + "/" + filename;

        // Check if temp source file exists
        bool srcExists = std::filesystem::exists(filePath);
        if (!srcExists) {
            LOG_ERROR("Avatar source file does not exist: %s", filePath.c_str());
        }

        // Copy file to permanent location
        try {
            std::filesystem::copy(filePath, destPath,
                                  std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error& fe) {
            LOG_ERROR("Failed to copy avatar file from %s to %s: %s",
                      filePath.c_str(), destPath.c_str(), fe.what());
            throw ServiceException("头像文件保存失败");
        }

        // Build URL: HTTP 路由路径（与服务路由 R"(/uploads/avatars/(.+))" 匹配）
        std::string avatarUrl = "/uploads/avatars/" + filename;

        // Update user's avatar_url in database
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE users SET avatar_url = $1 WHERE id = $2 | $1=%s $2=%d", avatarUrl.c_str(), userId);
        txn.exec(
            "UPDATE users SET avatar_url = $1 WHERE id = $2",
            pqxx::params{avatarUrl, userId});
        txn.commit();

        // Clean up temp file
        std::filesystem::remove(filePath);

        AvatarUploadResponse resp;
        resp.avatar_id = userId;   // Use userId as avatar resource identifier
        resp.avatar_url = avatarUrl;
        return resp;

    } catch (const std::exception& e) {
        LOG_ERROR("Database error in uploadAvatar: %s", e.what());
        throw ServiceException("头像上传失败");
    }
}

UserPreferences PgUserRepository::getPreferences(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT preference_type, value FROM user_preferences WHERE user_id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT preference_type, value FROM user_preferences "
            "WHERE user_id = $1 ORDER BY preference_type, value",
            pqxx::params{userId});
        txn.commit();

        UserPreferences prefs;
        for (const auto& row : r) {
            std::string type = row["preference_type"].c_str();
            std::string value = row["value"].c_str();
            if (type == "likes") {
                prefs.likes.push_back(value);
            } else if (type == "dislikes" || type == "allergies") {
                // v2.8: "allergies" and "dislikes" both map to dislikes
                prefs.dislikes.push_back(value);
            } else if (type == "health_goal" && prefs.health_goal.empty()) {
                prefs.health_goal = value;
            }
        }
        return prefs;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getPreferences: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

void PgUserRepository::updatePreferences(int userId, const UserPreferences& prefs) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // Delete all existing preferences for this user
        LOG_DEBUG("[SQL] DELETE FROM user_preferences WHERE user_id = $1 | $1=%d", userId);
        txn.exec("DELETE FROM user_preferences WHERE user_id = $1", pqxx::params{userId});

        // Insert likes
        for (const auto& v : prefs.likes) {
            LOG_DEBUG("[SQL] INSERT user_preferences (likes) | $1=%d", userId);
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'likes', $2)",
                pqxx::params{userId, v});
        }

        // Insert dislikes
        for (const auto& v : prefs.dislikes) {
            LOG_DEBUG("[SQL] INSERT user_preferences (dislikes) | $1=%d", userId);
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'dislikes', $2)",
                pqxx::params{userId, v});
        }

        // Insert health goal (if non-empty)
        if (!prefs.health_goal.empty()) {
            LOG_DEBUG("[SQL] INSERT user_preferences (health_goal) | $1=%d $2=%s", userId, prefs.health_goal.c_str());
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'health_goal', $2)",
                pqxx::params{userId, prefs.health_goal});
        }

        // Update preferences_complete flag on users table
        bool hasPrefs = !prefs.likes.empty() || !prefs.dislikes.empty() || !prefs.health_goal.empty();
        LOG_DEBUG("[SQL] UPDATE users SET preferences_complete = $1 WHERE id = $2 | $1=%d $2=%d", hasPrefs, userId);
        txn.exec(
            "UPDATE users SET preferences_complete = $1 WHERE id = $2",
            pqxx::params{hasPrefs, userId});

        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in updatePreferences: %s", e.what());
        throw ServiceException("偏好保存失败");
    }
}

HealthProfileResponse PgUserRepository::updateHealthProfile(int userId, const HealthProfileRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // Build conditions JSON array
        std::string conditionsJson = "[]";
        if (!req.conditions.empty()) {
            json arr = json::array();
            for (const auto& c : req.conditions)
                arr.push_back(c);
            conditionsJson = arr.dump();
        }

        // Two-step approach: ensure a row exists, then update fields
        LOG_DEBUG("[SQL] INSERT/UPDATE health_profiles (user_id=$1, conditions=...) | $1=%d", userId);
        txn.exec(
            "INSERT INTO health_profiles (user_id, conditions, created_at, updated_at) "
            "VALUES ($1, $2::jsonb, NOW(), NOW()) "
            "ON CONFLICT (user_id) DO UPDATE SET conditions = $2::jsonb, updated_at = NOW()",
            pqxx::params{userId, conditionsJson});

        // Then update individual fields if provided
        if (req.height_cm.has_value()) {
            LOG_DEBUG("[SQL] UPDATE health_profiles SET height_cm = $1 WHERE user_id = $2 | $2=%d", userId);
            txn.exec(
                "UPDATE health_profiles SET height_cm = $1, updated_at = NOW() WHERE user_id = $2",
                pqxx::params{req.height_cm.value(), userId});
        }
        if (req.weight_kg.has_value()) {
            LOG_DEBUG("[SQL] UPDATE health_profiles SET weight_kg = $1 WHERE user_id = $2 | $2=%d", userId);
            txn.exec(
                "UPDATE health_profiles SET weight_kg = $1, updated_at = NOW() WHERE user_id = $2",
                pqxx::params{req.weight_kg.value(), userId});
        }

        txn.commit();
        return HealthProfileResponse{};
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in updateHealthProfile: %s", e.what());
        throw ServiceException("健康指标保存失败");
    }
}

HealthProfileResponse PgUserRepository::getHealthProfile(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] SELECT height_cm, weight_kg, conditions FROM health_profiles WHERE user_id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT height_cm, weight_kg, conditions FROM health_profiles WHERE user_id = $1",
            pqxx::params{userId});
        txn.commit();

        HealthProfileResponse resp;
        if (!r.empty()) {
            auto row = r[0];
            if (!row["height_cm"].is_null())
                resp.height_cm = row["height_cm"].as<int>();
            if (!row["weight_kg"].is_null())
                resp.weight_kg = row["weight_kg"].as<double>();

            std::string condJson = row["conditions"].c_str();
            if (!condJson.empty() && condJson != "[]") {
                json arr = json::parse(condJson);
                for (const auto& c : arr)
                    resp.conditions.push_back(c.get<std::string>());
            }
        }
        return resp;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getHealthProfile: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

PagedFavorites PgUserRepository::getFavorites(int userId, int page, int size, const std::string& group) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        bool filterByDefault = (!group.empty() && group == "默认收藏夹");
        bool filterByGroup = (!group.empty() && !filterByDefault);
        int offset = (page - 1) * size;

        PagedFavorites result;

        if (filterByGroup) {
            // Group filter: $1=userId, $2=groupName, $3=limit, $4=offset
            LOG_DEBUG("[SQL] getFavorites(group filter) | $1=%d $2=%s", userId, group.c_str());
            pqxx::result countR = txn.exec(
                "SELECT COUNT(*) FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1 AND g.name = $2",
                pqxx::params{userId, group});
            pqxx::result dataR = txn.exec(
                "SELECT f.id, r.id AS recipe_id, r.name, COALESCE(r.description,'') AS description, "
                "COALESCE(r.image_url,'') AS image_url, "
                "COALESCE(g.name,'默认收藏夹') AS group_name, "
                "f.is_public, f.created_at::text AS favorited_at "
                "FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1 AND g.name = $2 "
                "ORDER BY f.created_at DESC LIMIT $3 OFFSET $4",
                pqxx::params{userId, group, size, offset});
            result.pagination.total = countR[0][0].as<int>();
            for (const auto& row : dataR) {
                FavoriteItem item;
                item.id = row["id"].as<int>();
                item.recipe_id = row["recipe_id"].as<int>();
                item.name = row["name"].c_str();
                item.description = row["description"].c_str();
                item.image_url = row["image_url"].c_str();
                item.group_name = row["group_name"].c_str();
                item.is_public = row["is_public"].as<bool>();
                item.favorited_at = row["favorited_at"].c_str();
                result.data.push_back(item);
            }
        } else if (filterByDefault) {
            // Default group (no group): $1=userId, $2=limit, $3=offset
            LOG_DEBUG("[SQL] getFavorites(default filter) | $1=%d", userId);
            pqxx::result countR = txn.exec(
                "SELECT COUNT(*) FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1 AND f.group_id IS NULL",
                pqxx::params{userId});
            pqxx::result dataR = txn.exec(
                "SELECT f.id, r.id AS recipe_id, r.name, COALESCE(r.description,'') AS description, "
                "COALESCE(r.image_url,'') AS image_url, "
                "COALESCE(g.name,'默认收藏夹') AS group_name, "
                "f.is_public, f.created_at::text AS favorited_at "
                "FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1 AND f.group_id IS NULL "
                "ORDER BY f.created_at DESC LIMIT $2 OFFSET $3",
                pqxx::params{userId, size, offset});
            result.pagination.total = countR[0][0].as<int>();
            for (const auto& row : dataR) {
                FavoriteItem item;
                item.id = row["id"].as<int>();
                item.recipe_id = row["recipe_id"].as<int>();
                item.name = row["name"].c_str();
                item.description = row["description"].c_str();
                item.image_url = row["image_url"].c_str();
                item.group_name = row["group_name"].c_str();
                item.is_public = row["is_public"].as<bool>();
                item.favorited_at = row["favorited_at"].c_str();
                result.data.push_back(item);
            }
        } else {
            // No group filter: $1=userId, $2=limit, $3=offset
            LOG_DEBUG("[SQL] getFavorites(no filter) | $1=%d", userId);
            pqxx::result countR = txn.exec(
                "SELECT COUNT(*) FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1",
                pqxx::params{userId});
            pqxx::result dataR = txn.exec(
                "SELECT f.id, r.id AS recipe_id, r.name, COALESCE(r.description,'') AS description, "
                "COALESCE(r.image_url,'') AS image_url, "
                "COALESCE(g.name,'默认收藏夹') AS group_name, "
                "f.is_public, f.created_at::text AS favorited_at "
                "FROM favorites f "
                "JOIN recipes r ON f.recipe_id = r.id "
                "LEFT JOIN favorite_groups g ON f.group_id = g.id "
                "WHERE f.user_id = $1 "
                "ORDER BY f.created_at DESC LIMIT $2 OFFSET $3",
                pqxx::params{userId, size, offset});
            result.pagination.total = countR[0][0].as<int>();
            for (const auto& row : dataR) {
                FavoriteItem item;
                item.id = row["id"].as<int>();
                item.recipe_id = row["recipe_id"].as<int>();
                item.name = row["name"].c_str();
                item.description = row["description"].c_str();
                item.image_url = row["image_url"].c_str();
                item.group_name = row["group_name"].c_str();
                item.is_public = row["is_public"].as<bool>();
                item.favorited_at = row["favorited_at"].c_str();
                result.data.push_back(item);
            }
        }

        txn.commit();
        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total_pages = (result.pagination.total + size - 1) / size;
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getFavorites: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

std::vector<FavoriteGroup> PgUserRepository::getFavoriteGroups(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // Count favorites in the default group (group_id IS NULL)
        LOG_DEBUG("[SQL] SELECT COUNT(*) FROM favorites WHERE user_id = $1 AND group_id IS NULL | $1=%d", userId);
        pqxx::result defaultCount = txn.exec(
            "SELECT COUNT(*) FROM favorites WHERE user_id = $1 AND group_id IS NULL",
            pqxx::params{userId});

        LOG_DEBUG("[SQL] SELECT g.id, g.name, ... FROM favorite_groups g ... WHERE g.user_id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT g.id, g.name, g.sort_order, COUNT(f.id) AS count "
            "FROM favorite_groups g "
            "LEFT JOIN favorites f ON f.group_id = g.id "
            "WHERE g.user_id = $1 "
            "GROUP BY g.id, g.name, g.sort_order "
            "ORDER BY g.sort_order",
            pqxx::params{userId});
        txn.commit();

        // Synthetic default group (always present)
        std::vector<FavoriteGroup> groups;
        FavoriteGroup defaultGroup;
        defaultGroup.id = 0;
        defaultGroup.name = "默认收藏夹";
        defaultGroup.sort_order = 0;
        defaultGroup.count = defaultCount[0][0].as<int>();
        groups.push_back(defaultGroup);

        for (const auto& row : r) {
            FavoriteGroup g;
            g.id = row["id"].as<int>();
            g.name = row["name"].c_str();
            g.sort_order = row["sort_order"].as<int>();
            g.count = row["count"].as<int>();
            groups.push_back(g);
        }
        return groups;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getFavoriteGroups: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

FavoriteGroup PgUserRepository::createFavoriteGroup(int userId, const CreateGroupRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        // Check for duplicate name
        LOG_DEBUG("[SQL] SELECT id FROM favorite_groups WHERE user_id = $1 AND name = $2 | $1=%d", userId);
        pqxx::result dupCheck = txn.exec(
            "SELECT id FROM favorite_groups WHERE user_id = $1 AND name = $2",
            pqxx::params{userId, req.name});
        if (!dupCheck.empty())
            throw ServiceException("分组名已存在", 409);

        // Get next sort_order
        LOG_DEBUG("[SQL] SELECT COALESCE(MAX(sort_order),... ) FROM favorite_groups WHERE user_id = $1 | $1=%d", userId);
        pqxx::result maxR = txn.exec(
            "SELECT COALESCE(MAX(sort_order), 0) + 1 FROM favorite_groups WHERE user_id = $1",
            pqxx::params{userId});
        int nextOrder = maxR[0][0].as<int>();

        // Advance sequence to avoid PK conflict with seed data
        LOG_DEBUG("[SQL] SELECT setval('favorite_groups_id_seq', ...)");
        txn.exec(
            "SELECT setval('favorite_groups_id_seq', COALESCE((SELECT MAX(id) FROM favorite_groups), 0) + 1, false)"
        );

        LOG_DEBUG("[SQL] INSERT INTO favorite_groups ... RETURNING id, name, sort_order | $1=%d", userId);
        pqxx::result r = txn.exec(
            "INSERT INTO favorite_groups (user_id, name, sort_order) VALUES ($1, $2, $3) RETURNING id, name, sort_order",
            pqxx::params{userId, req.name, nextOrder});
        txn.commit();

        FavoriteGroup group;
        group.id = r[0]["id"].as<int>();
        group.name = r[0]["name"].c_str();
        group.sort_order = r[0]["sort_order"].as<int>();
        group.count = 0;
        return group;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in createFavoriteGroup: %s", e.what());
        throw ServiceException("创建分组失败");
    }
}

void PgUserRepository::updateFavoriteGroup(int userId, int groupId, const UpdateGroupRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE favorite_groups SET name = $1 WHERE id = $2 AND user_id = $3 | $2=%d $3=%d", groupId, userId);
        auto r = txn.exec(
            "UPDATE favorite_groups SET name = $1 WHERE id = $2 AND user_id = $3",
            pqxx::params{req.name, groupId, userId});
        if (r.affected_rows() == 0)
            throw ServiceException("分组不存在", 404);
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in updateFavoriteGroup: %s", e.what());
        throw ServiceException("更新分组失败");
    }
}

void PgUserRepository::deleteFavoriteGroup(int userId, int groupId) {
    if (groupId <= 0)
        throw ServiceException("默认分组不可删除", 400);
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        // Delete all favorites in the group (not move to default)
        LOG_DEBUG("[SQL] DELETE FROM favorites WHERE group_id = $1 AND user_id = $2 | $1=%d $2=%d", groupId, userId);
        txn.exec(
            "DELETE FROM favorites WHERE group_id = $1 AND user_id = $2",
            pqxx::params{groupId, userId});
        // Delete the group
        LOG_DEBUG("[SQL] DELETE FROM favorite_groups WHERE id = $1 AND user_id = $2 | $1=%d $2=%d", groupId, userId);
        auto r = txn.exec(
            "DELETE FROM favorite_groups WHERE id = $1 AND user_id = $2",
            pqxx::params{groupId, userId});
        if (r.affected_rows() == 0)
            throw ServiceException("分组不存在", 404);
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in deleteFavoriteGroup: %s", e.what());
        throw ServiceException("删除分组失败");
    }
}

void PgUserRepository::updateFavoriteItem(int userId, int favoriteId, const UpdateFavoriteRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        if (req.group_id.has_value()) {
            int gid = req.group_id.value();
            if (gid > 0) {
                // Verify the group belongs to the user
                LOG_DEBUG("[SQL] SELECT id FROM favorite_groups WHERE id = $1 AND user_id = $2 | $1=%d", gid);
                auto g = txn.exec(
                    "SELECT id FROM favorite_groups WHERE id = $1 AND user_id = $2",
                    pqxx::params{gid, userId});
                if (g.empty())
                    throw ServiceException("分组不存在", 404);
                LOG_DEBUG("[SQL] UPDATE favorites SET group_id = $1 WHERE id = $2 AND user_id = $3 | $1=%d $2=%d", gid, favoriteId);
                txn.exec(
                    "UPDATE favorites SET group_id = $1 WHERE id = $2 AND user_id = $3",
                    pqxx::params{gid, favoriteId, userId});
            } else {
                // group_id = 0 表示移到默认收藏夹（设置 NULL）
                LOG_DEBUG("[SQL] UPDATE favorites SET group_id = NULL WHERE id = $1 AND user_id = $2 | $1=%d", favoriteId);
                txn.exec(
                    "UPDATE favorites SET group_id = NULL WHERE id = $1 AND user_id = $2",
                    pqxx::params{favoriteId, userId});
            }
        }
        if (req.is_public.has_value()) {
            LOG_DEBUG("[SQL] UPDATE favorites SET is_public = $1 WHERE id = $2 AND user_id = $3 | $2=%d", favoriteId);
            txn.exec(
                "UPDATE favorites SET is_public = $1 WHERE id = $2 AND user_id = $3",
                pqxx::params{req.is_public.value(), favoriteId, userId});
        }
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in updateFavoriteItem: %s", e.what());
        throw ServiceException("更新收藏项失败");
    }
}

void PgUserRepository::batchDeleteFavorites(int userId, const BatchDeleteFavoritesRequest& req) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        for (const auto& fid : req.favorite_ids) {
            LOG_DEBUG("[SQL] DELETE FROM favorites WHERE id = $1 AND user_id = $2 | $1=%d", fid);
            txn.exec(
                "DELETE FROM favorites WHERE id = $1 AND user_id = $2",
                pqxx::params{fid, userId});
        }
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in batchDeleteFavorites: %s", e.what());
        throw ServiceException("批量删除失败");
    }
}

PagedNotifications PgUserRepository::getNotifications(int userId, int page, int size, const std::string& type) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

        bool filterByType = !type.empty();
        int offset = (page - 1) * size;

        PagedNotifications result;

        if (filterByType) {
            // Type filter: $1=userId, $2=type, $3=limit, $4=offset
            LOG_DEBUG("[SQL] getNotifications(type filter) | $1=%d $2=%s", userId, type.c_str());
            pqxx::result countR = txn.exec(
                "SELECT COUNT(*) FROM notifications WHERE user_id = $1 AND type = $2",
                pqxx::params{userId, type});
            pqxx::result dataR = txn.exec(
                "SELECT id, title, content, type, sub_type, is_read, "
                "related_id, trigger_user_name, created_at::text "
                "FROM notifications WHERE user_id = $1 AND type = $2 "
                "ORDER BY created_at DESC LIMIT $3 OFFSET $4",
                pqxx::params{userId, type, size, offset});
            result.pagination.total = countR[0][0].as<int>();
            for (const auto& row : dataR) {
                NotificationItem item;
                item.id = row["id"].as<int>();
                item.title = row["title"].as<std::string>();
                item.content = row["content"].as<std::string>();
                item.type = row["type"].as<std::string>();
                if (!row["sub_type"].is_null())
                    item.sub_type = row["sub_type"].as<std::string>();
                item.is_read = row["is_read"].as<bool>();
                if (!row["related_id"].is_null())
                    item.related_id = row["related_id"].as<int>();
                if (!row["trigger_user_name"].is_null())
                    item.trigger_user_name = row["trigger_user_name"].as<std::string>();
                item.created_at = row["created_at"].as<std::string>();
                result.data.push_back(item);
            }
        } else {
            // No type filter: $1=userId, $2=limit, $3=offset
            LOG_DEBUG("[SQL] getNotifications(no filter) | $1=%d", userId);
            pqxx::result countR = txn.exec(
                "SELECT COUNT(*) FROM notifications WHERE user_id = $1",
                pqxx::params{userId});
            pqxx::result dataR = txn.exec(
                "SELECT id, title, content, type, sub_type, is_read, "
                "related_id, trigger_user_name, created_at::text "
                "FROM notifications WHERE user_id = $1 "
                "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
                pqxx::params{userId, size, offset});
            result.pagination.total = countR[0][0].as<int>();
            for (const auto& row : dataR) {
                NotificationItem item;
                item.id = row["id"].as<int>();
                item.title = row["title"].as<std::string>();
                item.content = row["content"].as<std::string>();
                item.type = row["type"].as<std::string>();
                if (!row["sub_type"].is_null())
                    item.sub_type = row["sub_type"].as<std::string>();
                item.is_read = row["is_read"].as<bool>();
                if (!row["related_id"].is_null())
                    item.related_id = row["related_id"].as<int>();
                if (!row["trigger_user_name"].is_null())
                    item.trigger_user_name = row["trigger_user_name"].as<std::string>();
                item.created_at = row["created_at"].as<std::string>();
                result.data.push_back(item);
            }
        }

        txn.commit();
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in getNotifications: %s", e.what());
        throw ServiceException("获取通知列表失败");
    }
}

void PgUserRepository::markNotificationRead(int userId, int notificationId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE notifications SET is_read = true WHERE id = $1 AND user_id = $2 | $1=%d", notificationId);
        auto r = txn.exec(
            "UPDATE notifications SET is_read = true WHERE id = $1 AND user_id = $2",
            pqxx::params{notificationId, userId});
        if (r.affected_rows() == 0) {
            txn.commit();
            throw ServiceException("通知不存在", 404);
        }
        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in markNotificationRead: %s", e.what());
        throw ServiceException("标记已读失败");
    }
}

void PgUserRepository::markAllNotificationsRead(int userId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE notifications SET is_read = true WHERE user_id = $1 AND is_read = false | $1=%d", userId);
        txn.exec(
            "UPDATE notifications SET is_read = true WHERE user_id = $1 AND is_read = false",
            pqxx::params{userId});
        txn.commit();
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in markAllNotificationsRead: %s", e.what());
        throw ServiceException("全部标记已读失败");
    }
}

void PgUserRepository::deleteNotification(int userId, int notificationId) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] DELETE FROM notifications WHERE id = $1 AND user_id = $2 | $1=%d", notificationId);
        auto r = txn.exec(
            "DELETE FROM notifications WHERE id = $1 AND user_id = $2",
            pqxx::params{notificationId, userId});
        if (r.affected_rows() == 0) {
            txn.commit();
            throw ServiceException("通知不存在", 404);
        }
        txn.commit();
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error in deleteNotification: %s", e.what());
        throw ServiceException("删除通知失败");
    }
}
#pragma GCC diagnostic pop
