#include "PgUserRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include "../common/DbExecutor.h"
#include "../common/UploadPaths.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cctype>

using json = nlohmann::json;
using namespace gocook::models;
using namespace gocook::repository;
using namespace gocook::services;

//   本文件已按"生产级收敛形态"重构：每个方法用 executeDb 包裹（见 ../common/DbExecutor.h），
//   方法体只剩"差异部分"（SQL + 参数 + 行→结构体映射），异常分层/事务边界由辅助函数统一保证。
//   两个特例保持手写（原因见方法内注释）：getHealthConditions（吞错误返回空）、
//   uploadAvatar（文件操作 + 数据库混编 + 自定义错误文案）。

std::optional<UserAuthInfo> PgUserRepository::findByUsername(const std::string& username) {
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<UserAuthInfo> {
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
        return info;
    }, "数据库操作失败");
}

bool PgUserRepository::existsByEmail(const std::string& email) {
    return executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE email = $1 (existsByEmail) | $1=%s", email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE email = $1", pqxx::params{email});
        return !r.empty();
    }, "数据库操作失败");
}

void PgUserRepository::createUser(const std::string& username,
                                  const std::string& passwordHash,
                                  const std::string& email) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3) | $1=%s $3=%s",
                 username.c_str(), email.c_str());
        txn.exec(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3)",
            pqxx::params{username, passwordHash, email});
    }, "数据库操作失败");
}

void PgUserRepository::upsertPendingRegistration(const std::string& username,
                                                 const std::string& passwordHash,
                                                 const std::string& email,
                                                 const std::string& token) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 邮箱唯一：重复提交覆盖旧记录并重置过期时间（旧验证码随之失效）
        LOG_DEBUG("[SQL] UPSERT pending_registrations | email=%s", email.c_str());
        txn.exec(
            "INSERT INTO pending_registrations (username, password_hash, email, token, expires_at) "
            "VALUES ($1, $2, $3, $4, NOW() + INTERVAL '" "15 minutes')"
            "ON CONFLICT (email) DO UPDATE SET "
            "    username = EXCLUDED.username, password_hash = EXCLUDED.password_hash, "
            "    token = EXCLUDED.token, expires_at = EXCLUDED.expires_at, created_at = NOW()",
            pqxx::params{username, passwordHash, email, token});
    }, "数据库操作失败");
}

gocook::models::RegistrationOutcome PgUserRepository::createUserFromPendingRegistration(
    const std::string& email, const std::string& token) {
    return executeDb(db_, [&](pqxx::work& txn) -> gocook::models::RegistrationOutcome {
        // 1) 锁定并校验待验证记录（未过期 + 验证码一致）
        LOG_DEBUG("[SQL] SELECT pending_registrations | email=%s", email.c_str());
        pqxx::result pending = txn.exec(
            "SELECT username, password_hash FROM pending_registrations "
            "WHERE email = $1 AND token = $2 AND expires_at > NOW() FOR UPDATE",
            pqxx::params{email, token});
        if (pending.empty()) return gocook::models::RegistrationOutcome::InvalidCode;

        std::string username = pending[0]["username"].c_str();
        std::string passwordHash = pending[0]["password_hash"].c_str();

        // 2) 唯一性复核（提交验证码期间可能存在竞态）
        pqxx::result u = txn.exec("SELECT 1 FROM users WHERE username = $1", pqxx::params{username});
        if (!u.empty()) return gocook::models::RegistrationOutcome::UsernameTaken;
        pqxx::result e = txn.exec("SELECT 1 FROM users WHERE email = $1", pqxx::params{email});
        if (!e.empty()) return gocook::models::RegistrationOutcome::EmailTaken;

        // 3) 建号并删除待验证记录（同一事务，提交后原子生效）
        txn.exec("INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3)",
                 pqxx::params{username, passwordHash, email});
        txn.exec("DELETE FROM pending_registrations WHERE email = $1", pqxx::params{email});
        return gocook::models::RegistrationOutcome::Success;
    }, "数据库操作失败");
}

std::optional<UserProfile> PgUserRepository::findById(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<UserProfile> {

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
        return u;
    }, "数据库操作失败");
}

std::optional<int> PgUserRepository::findIdByEmail(const std::string& email) {
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<int> {
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE email = $1 (findIdByEmail) | $1=%s", email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE email = $1", pqxx::params{email});
        if (r.empty()) return std::nullopt;
        return r[0]["id"].as<int>();
    }, "数据库操作失败");
}

std::optional<int> PgUserRepository::findIdByUsernameAndEmail(
    const std::string& username, const std::string& email)
{
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<int> {
        LOG_DEBUG("[SQL] SELECT id FROM users WHERE username = $1 AND email = $2 | $1=%s $2=%s",
                 username.c_str(), email.c_str());
        pqxx::result r = txn.exec(
            "SELECT id FROM users WHERE username = $1 AND email = $2",
            pqxx::params{username, email});
        if (r.empty()) return std::nullopt;
        return r[0]["id"].as<int>();
    }, "数据库操作失败");
}

void PgUserRepository::createPasswordResetToken(
    int userId, const std::string& token)
{
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] INSERT INTO password_reset_tokens (user_id, token, expires_at) VALUES ($1, $2, ...) | $1=%d", userId);
        txn.exec(
            "INSERT INTO password_reset_tokens (user_id, token, expires_at) "
            "VALUES ($1, $2, NOW() + INTERVAL '" + std::to_string(TOKEN_EXPIRY_MINUTES) + " minutes')",
            pqxx::params{userId, token});
    }, "创建重置令牌失败");
}

std::optional<int> PgUserRepository::findUserIdByResetToken(const std::string& token) {
    return executeDb(db_, [&](pqxx::work& txn) -> std::optional<int> {
        LOG_DEBUG("[SQL] SELECT user_id FROM password_reset_tokens WHERE token = $1 ...");
        pqxx::result r = txn.exec(
            "SELECT user_id FROM password_reset_tokens "
            "WHERE token = $1 AND used = false AND expires_at > NOW()",
            pqxx::params{token});
        if (r.empty()) return std::nullopt;
        return r[0]["user_id"].as<int>();
    }, "数据库操作失败");
}

void PgUserRepository::markResetTokenUsed(const std::string& token) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] UPDATE password_reset_tokens SET used = true WHERE token = $1");
        txn.exec(
            "UPDATE password_reset_tokens SET used = true WHERE token = $1",
            pqxx::params{token});
    }, "数据库操作失败");
}

void PgUserRepository::resetPasswordAndMarkTokenUsed(
    int userId, const std::string& newPasswordHash, const std::string& token)
{
    executeDb(db_, [&](pqxx::work& txn) {
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
    }, "数据库操作失败");
    LOG_INFO("用户 %d 的密码已重置，重置令牌已标记为已使用", userId);
}

void PgUserRepository::updateProfile(int userId, const UpdateProfileRequest& profile) {
    executeDb(db_, [&](pqxx::work& txn) {
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
    }, "数据库操作失败");
}

// ⚠️ 特例：错误处理策略是"吞掉 DB 异常、返回空列表"（健康条件是可选数据，查不到不该让上层
// 流程崩），且刻意不 commit（只读）。与 executeDb 的"统一翻译成异常"策略不同，故保持手写。
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
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("获取健康条件失败：%s", e.what());
        return {};
    }
}

std::string PgUserRepository::getPasswordHash(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] SELECT password_hash FROM users WHERE id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT password_hash FROM users WHERE id = $1", pqxx::params{userId});
        if (r.empty()) {
            throw ServiceException("用户不存在", 404);
        }
        return std::string(r[0]["password_hash"].c_str());
    }, "数据库操作失败");
}

void PgUserRepository::changePassword(int userId, const std::string& newPasswordHash) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] UPDATE users SET password_hash = $1 WHERE id = $2 | $2=%d", userId);
        auto r = txn.exec(
            "UPDATE users SET password_hash = $1 WHERE id = $2",
            pqxx::params{newPasswordHash, userId});
        if (r.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }
    }, "数据库操作失败");
}

void PgUserRepository::deleteAccount(int userId) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 1. 匿名化公开内容：菜谱（author_id 无 CASCADE，注销后作者置空而不是连菜谱一起删）
        LOG_DEBUG("[SQL] UPDATE recipes SET author_id = NULL WHERE author_id = $1 | $1=%d", userId);
        txn.exec("UPDATE recipes SET author_id = NULL WHERE author_id = $1", pqxx::params{userId});
        //    评分：ON DELETE CASCADE + NOT NULL → 随用户一起删掉，无需单独处理

        // 2. 删除用户——外键 CASCADE 自动清干净以下关联表：
        //    user_preferences, health_profiles, inventory, shopping_lists,
        //    shopping_list_items, notifications, meal_plans, favorites, favorite_groups
        LOG_DEBUG("[SQL] DELETE FROM users WHERE id = $1 | $1=%d", userId);
        auto delResult = txn.exec(
            "DELETE FROM users WHERE id = $1", pqxx::params{userId});
        if (delResult.affected_rows() == 0) {
            throw ServiceException("用户不存在", 404);
        }
    }, "账户注销失败");
}

// ⚠️ 特例：文件操作（拷贝/清理）+ 数据库更新混编，且失败文案是自定义的"头像上传失败"。
// 保持手写——特例显式化，比给 executeDb 加"错误处理策略"参数更清晰。
AvatarUploadResponse PgUserRepository::uploadAvatar(int userId, const std::string& filePath) {
    try {
        // 根据上传文件的后缀确定扩展名
        std::string ext = ".jpg";  // 默认
        auto dotPos = filePath.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string origExt = filePath.substr(dotPos);
            // 统一转小写再比较
            std::string lower;
            for (char c : origExt) lower += std::tolower(static_cast<unsigned char>(c));
            if (lower == ".png")      ext = ".png";
            else if (lower == ".gif") ext = ".gif";
            else if (lower == ".bmp") ext = ".bmp";
            else if (lower == ".svg")  ext = ".svg";
        }

        // 生成唯一文件名：user_<id>_<时间戳><扩展名>
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                      now.time_since_epoch()).count();
        std::string filename = "user_" + std::to_string(userId)
                             + "_" + std::to_string(ts) + ext;

        // 按需创建上传目录（绝对路径，与 Router 文件服务路径一致，规则见 ../common/UploadPaths.h）
        std::string uploadDir = UploadPaths::baseDir() + "/avatars/";
        std::filesystem::create_directories(uploadDir);
        std::string destPath = uploadDir + "/" + filename;

        // 检查临时源文件是否存在
        bool srcExists = std::filesystem::exists(filePath);
        if (!srcExists) {
            LOG_ERROR("头像源文件不存在：%s", filePath.c_str());
        }

        // 把文件复制到永久位置
        try {
            std::filesystem::copy(filePath, destPath,
                                  std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error& fe) {
            LOG_ERROR("复制头像文件失败（%s → %s）：%s",
                      filePath.c_str(), destPath.c_str(), fe.what());
            throw ServiceException("头像文件保存失败");
        }

        // 拼接 URL：HTTP 路由路径（与服务路由 R"(/uploads/avatars/(.+))" 匹配）
        std::string avatarUrl = "/uploads/avatars/" + filename;

        // 更新数据库里的 avatar_url（数据库部分也可以单独走 executeDb，
        // 但整个方法保持手写更直观——文件失败文案与 DB 失败文案不同）
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);
        LOG_DEBUG("[SQL] UPDATE users SET avatar_url = $1 WHERE id = $2 | $1=%s $2=%d", avatarUrl.c_str(), userId);
        txn.exec(
            "UPDATE users SET avatar_url = $1 WHERE id = $2",
            pqxx::params{avatarUrl, userId});
        txn.commit();

        // 清理临时文件
        std::filesystem::remove(filePath);

        AvatarUploadResponse resp;
        resp.avatar_id = userId;   // 用 userId 作为头像资源标识
        resp.avatar_url = avatarUrl;
        return resp;

    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN("头像上传时数据库出错：%s", e.what());
        throw ServiceException("头像上传失败");
    }
}

UserPreferences PgUserRepository::getPreferences(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] SELECT preference_type, value FROM user_preferences WHERE user_id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT preference_type, value FROM user_preferences "
            "WHERE user_id = $1 ORDER BY preference_type, value",
            pqxx::params{userId});

        UserPreferences prefs;
        for (const auto& row : r) {
            std::string type = row["preference_type"].c_str();
            std::string value = row["value"].c_str();
            if (type == "likes") {
                prefs.likes.push_back(value);
            } else if (type == "dislikes" || type == "allergies") {
                // v2.8：allergies 和 dislikes 都归入 dislikes
                prefs.dislikes.push_back(value);
            } else if (type == "health_goal" && prefs.health_goal.empty()) {
                prefs.health_goal = value;
            }
        }
        return prefs;
    }, "数据库操作失败");
}

void PgUserRepository::updatePreferences(int userId, const UserPreferences& prefs) {
    executeDb(db_, [&](pqxx::work& txn) {
        // 先删除该用户的全部旧偏好
        LOG_DEBUG("[SQL] DELETE FROM user_preferences WHERE user_id = $1 | $1=%d", userId);
        txn.exec("DELETE FROM user_preferences WHERE user_id = $1", pqxx::params{userId});

        // 插入 likes
        for (const auto& v : prefs.likes) {
            LOG_DEBUG("[SQL] INSERT user_preferences (likes) | $1=%d", userId);
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'likes', $2)",
                pqxx::params{userId, v});
        }

        // 插入 dislikes
        for (const auto& v : prefs.dislikes) {
            LOG_DEBUG("[SQL] INSERT user_preferences (dislikes) | $1=%d", userId);
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'dislikes', $2)",
                pqxx::params{userId, v});
        }

        // 插入健康目标（非空才插）
        if (!prefs.health_goal.empty()) {
            LOG_DEBUG("[SQL] INSERT user_preferences (health_goal) | $1=%d $2=%s", userId, prefs.health_goal.c_str());
            txn.exec(
                "INSERT INTO user_preferences (user_id, preference_type, value) VALUES ($1, 'health_goal', $2)",
                pqxx::params{userId, prefs.health_goal});
        }

        // 同步 users 表的 preferences_complete 标记
        bool hasPrefs = !prefs.likes.empty() || !prefs.dislikes.empty() || !prefs.health_goal.empty();
        LOG_DEBUG("[SQL] UPDATE users SET preferences_complete = $1 WHERE id = $2 | $1=%d $2=%d", hasPrefs, userId);
        txn.exec(
            "UPDATE users SET preferences_complete = $1 WHERE id = $2",
            pqxx::params{hasPrefs, userId});
    }, "偏好保存失败");
}

HealthProfileResponse PgUserRepository::updateHealthProfile(int userId, const HealthProfileRequest& req) {
    return executeDb(db_, [&](pqxx::work& txn) -> HealthProfileResponse {
        // 把条件列表拼成 JSON 数组
        std::string conditionsJson = "[]";
        if (!req.conditions.empty()) {
            json arr = json::array();
            for (const auto& c : req.conditions)
                arr.push_back(c);
            conditionsJson = arr.dump();
        }

        // 两步法：先确保行存在（upsert），再更新可选字段
        LOG_DEBUG("[SQL] INSERT/UPDATE health_profiles (user_id=$1, conditions=...) | $1=%d", userId);
        txn.exec(
            "INSERT INTO health_profiles (user_id, conditions, created_at, updated_at) "
            "VALUES ($1, $2::jsonb, NOW(), NOW()) "
            "ON CONFLICT (user_id) DO UPDATE SET conditions = $2::jsonb, updated_at = NOW()",
            pqxx::params{userId, conditionsJson});

        // 再按传入的可选字段逐个更新
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
        return HealthProfileResponse{};
    }, "健康指标保存失败");
}

HealthProfileResponse PgUserRepository::getHealthProfile(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] SELECT height_cm, weight_kg, conditions FROM health_profiles WHERE user_id = $1 | $1=%d", userId);
        pqxx::result r = txn.exec(
            "SELECT height_cm, weight_kg, conditions FROM health_profiles WHERE user_id = $1",
            pqxx::params{userId});

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
    }, "数据库操作失败");
}

PagedFavorites PgUserRepository::getFavorites(int userId, int page, int size, const std::string& group) {
    return executeDb(db_, [&](pqxx::work& txn) {
        bool filterByDefault = (!group.empty() && group == "默认收藏夹");
        bool filterByGroup = (!group.empty() && !filterByDefault);
        int offset = (page - 1) * size;

        PagedFavorites result;

        if (filterByGroup) {
            // 分组筛选：$1=userId, $2=groupName, $3=limit, $4=offset
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
            // 默认收藏夹（无分组）：$1=userId, $2=limit, $3=offset
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
            // 无分组筛选：$1=userId, $2=limit, $3=offset
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

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total_pages = safeTotalPages(result.pagination.total, size);
        return result;
    }, "数据库操作失败");
}

std::vector<FavoriteGroup> PgUserRepository::getFavoriteGroups(int userId) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 统计默认收藏夹（group_id IS NULL）里的收藏数
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

        // 合成默认分组（永远存在，即使一个自定义分组都没有）
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
    }, "数据库操作失败");
}

FavoriteGroup PgUserRepository::createFavoriteGroup(int userId, const CreateGroupRequest& req) {
    return executeDb(db_, [&](pqxx::work& txn) {
        // 查重：同名分组已存在则 409
        LOG_DEBUG("[SQL] SELECT id FROM favorite_groups WHERE user_id = $1 AND name = $2 | $1=%d", userId);
        pqxx::result dupCheck = txn.exec(
            "SELECT id FROM favorite_groups WHERE user_id = $1 AND name = $2",
            pqxx::params{userId, req.name});
        if (!dupCheck.empty())
            throw ServiceException("分组名已存在", 409);

        // 取下一个排序号
        LOG_DEBUG("[SQL] SELECT COALESCE(MAX(sort_order),... ) FROM favorite_groups WHERE user_id = $1 | $1=%d", userId);
        pqxx::result maxR = txn.exec(
            "SELECT COALESCE(MAX(sort_order), 0) + 1 FROM favorite_groups WHERE user_id = $1",
            pqxx::params{userId});
        int nextOrder = maxR[0][0].as<int>();

        // 推进自增序列，避免与种子数据的主键冲突
        LOG_DEBUG("[SQL] SELECT setval('favorite_groups_id_seq', ...)");
        txn.exec(
            "SELECT setval('favorite_groups_id_seq', COALESCE((SELECT MAX(id) FROM favorite_groups), 0) + 1, false)"
        );

        LOG_DEBUG("[SQL] INSERT INTO favorite_groups ... RETURNING id, name, sort_order | $1=%d", userId);
        pqxx::result r = txn.exec(
            "INSERT INTO favorite_groups (user_id, name, sort_order) VALUES ($1, $2, $3) RETURNING id, name, sort_order",
            pqxx::params{userId, req.name, nextOrder});

        FavoriteGroup group;
        group.id = r[0]["id"].as<int>();
        group.name = r[0]["name"].c_str();
        group.sort_order = r[0]["sort_order"].as<int>();
        group.count = 0;
        return group;
    }, "创建分组失败");
}

void PgUserRepository::updateFavoriteGroup(int userId, int groupId, const UpdateGroupRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] UPDATE favorite_groups SET name = $1 WHERE id = $2 AND user_id = $3 | $2=%d $3=%d", groupId, userId);
        auto r = txn.exec(
            "UPDATE favorite_groups SET name = $1 WHERE id = $2 AND user_id = $3",
            pqxx::params{req.name, groupId, userId});
        if (r.affected_rows() == 0)
            throw ServiceException("分组不存在", 404);
    }, "更新分组失败");
}

void PgUserRepository::deleteFavoriteGroup(int userId, int groupId) {
    if (groupId <= 0)
        throw ServiceException("默认分组不可删除", 400);   // 纯参数校验，不碰数据库，放在 executeDb 外
    executeDb(db_, [&](pqxx::work& txn) {
        // 先删除组内所有收藏（不移到默认分组）
        LOG_DEBUG("[SQL] DELETE FROM favorites WHERE group_id = $1 AND user_id = $2 | $1=%d $2=%d", groupId, userId);
        txn.exec(
            "DELETE FROM favorites WHERE group_id = $1 AND user_id = $2",
            pqxx::params{groupId, userId});
        // 再删除分组本身
        LOG_DEBUG("[SQL] DELETE FROM favorite_groups WHERE id = $1 AND user_id = $2 | $1=%d $2=%d", groupId, userId);
        auto r = txn.exec(
            "DELETE FROM favorite_groups WHERE id = $1 AND user_id = $2",
            pqxx::params{groupId, userId});
        if (r.affected_rows() == 0)
            throw ServiceException("分组不存在", 404);
    }, "删除分组失败");
}

void PgUserRepository::updateFavoriteItem(int userId, int favoriteId, const UpdateFavoriteRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
        if (req.group_id.has_value()) {
            int gid = req.group_id.value();
            if (gid > 0) {
                // 校验该分组属于当前用户
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
    }, "更新收藏项失败");
}

void PgUserRepository::batchDeleteFavorites(int userId, const BatchDeleteFavoritesRequest& req) {
    executeDb(db_, [&](pqxx::work& txn) {
        for (const auto& fid : req.favorite_ids) {
            LOG_DEBUG("[SQL] DELETE FROM favorites WHERE id = $1 AND user_id = $2 | $1=%d", fid);
            txn.exec(
                "DELETE FROM favorites WHERE id = $1 AND user_id = $2",
                pqxx::params{fid, userId});
        }
    }, "批量删除失败");
}

PagedNotifications PgUserRepository::getNotifications(int userId, int page, int size, const std::string& type) {
    return executeDb(db_, [&](pqxx::work& txn) {
        bool filterByType = !type.empty();
        int offset = (page - 1) * size;

        PagedNotifications result;

        if (filterByType) {
            // 类型筛选：$1=userId, $2=type, $3=limit, $4=offset
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
            // 无类型筛选：$1=userId, $2=limit, $3=offset
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

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total_pages = safeTotalPages(result.pagination.total, size);
        return result;
    }, "获取通知列表失败");
}

void PgUserRepository::markNotificationRead(int userId, int notificationId) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] UPDATE notifications SET is_read = true WHERE id = $1 AND user_id = $2 | $1=%d", notificationId);
        auto r = txn.exec(
            "UPDATE notifications SET is_read = true WHERE id = $1 AND user_id = $2",
            pqxx::params{notificationId, userId});
        if (r.affected_rows() == 0) {
            throw ServiceException("通知不存在", 404);
            // 原代码这里先 commit 再抛（0 行影响，commit/回滚等价），已简化掉
        }
    }, "标记已读失败");
}

void PgUserRepository::markAllNotificationsRead(int userId) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] UPDATE notifications SET is_read = true WHERE user_id = $1 AND is_read = false | $1=%d", userId);
        txn.exec(
            "UPDATE notifications SET is_read = true WHERE user_id = $1 AND is_read = false",
            pqxx::params{userId});
    }, "全部标记已读失败");
}

void PgUserRepository::deleteNotification(int userId, int notificationId) {
    executeDb(db_, [&](pqxx::work& txn) {
        LOG_DEBUG("[SQL] DELETE FROM notifications WHERE id = $1 AND user_id = $2 | $1=%d", notificationId);
        auto r = txn.exec(
            "DELETE FROM notifications WHERE id = $1 AND user_id = $2",
            pqxx::params{notificationId, userId});
        if (r.affected_rows() == 0) {
            throw ServiceException("通知不存在", 404);
        }
    }, "删除通知失败");
}
