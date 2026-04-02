#include "UserHandler.h"
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

// 简单的 base64 编码函数（用于生成 token）
static std::string base64Encode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    int in_len = input.size();
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++)
                result += chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (int j = 0; j < i + 1; j++)
            result += chars[char_array_4[j]];
        while (i++ < 3)
            result += '=';
    }
    return result;
}

UserHandler::UserHandler(DBConnection& db) : db_(db) {}

void UserHandler::registerUser(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("username") || !reqJson.contains("password")) {
            res.status = 400;
            res.body = json{{"error", "Missing username or password"}}.dump();
            return;
        }
        std::string username = reqJson["username"];
        std::string password = reqJson["password"];
        if (username.empty() || password.empty()) {
            res.status = 400;
            res.body = json{{"error", "Username and password cannot be empty"}}.dump();
            return;
        }

        // 检查用户名是否已存在
        pqxx::work txn(db_.getConn());
        pqxx::result check = txn.exec_params("SELECT id FROM users WHERE username = $1", username);
        if (!check.empty()) {
            res.status = 409; // Conflict
            res.body = json{{"error", "Username already exists"}}.dump();
            return;
        }

        // 哈希密码（当前简单处理，直接存储明文，实际应使用 bcrypt）
        std::string hashed = hashPassword(password);

        // 插入新用户
        txn.exec_params("INSERT INTO users (username, password_hash) VALUES ($1, $2)", username, hashed);
        txn.commit();

        res.status = 201;
        res.body = json{{"message", "User registered successfully"}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void UserHandler::loginUser(const httplib::Request& req, httplib::Response& res) {
    try {
        json reqJson = json::parse(req.body);
        if (!reqJson.contains("username") || !reqJson.contains("password")) {
            res.status = 400;
            res.body = json{{"error", "Missing username or password"}}.dump();
            return;
        }
        std::string username = reqJson["username"];
        std::string password = reqJson["password"];

        pqxx::work txn(db_.getConn());
        pqxx::result result = txn.exec_params("SELECT id, password_hash FROM users WHERE username = $1", username);
        if (result.empty()) {
            res.status = 401;
            res.body = json{{"error", "Invalid username or password"}}.dump();
            return;
        }

        int userId = result[0]["id"].as<int>();
        std::string storedHash = result[0]["password_hash"].as<std::string>();

        if (!validatePassword(password, storedHash)) {
            res.status = 401;
            res.body = json{{"error", "Invalid username or password"}}.dump();
            return;
        }

        // 生成 token
        std::string token = generateToken(userId, username);
        res.status = 200;
        res.body = json{{"token", token}, {"user_id", userId}, {"username", username}}.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

void UserHandler::getUsersPublic(const httplib::Request& req, httplib::Response& res) {
    try {
        pqxx::work txn(db_.getConn());
        pqxx::result result = txn.exec("SELECT id, username, created_at FROM users ORDER BY id");

        json users = json::array();
        for (const auto& row : result) {
            json user;
            user["id"] = row["id"].as<int>();
            user["username"] = row["username"].as<std::string>();
            user["created_at"] = row["created_at"].as<std::string>();
            users.push_back(user);
        }
        txn.commit();

        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = users.dump();
    } catch (const std::exception& e) {
        res.status = 500;
        res.body = json{{"error", e.what()}}.dump();
    }
}

// ------------------ 私有辅助函数 ------------------
std::string UserHandler::generateToken(int userId, const std::string& username) {
    // 使用当前时间戳 + userId + username 生成一个简单的 token
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string raw = std::to_string(userId) + ":" + username + ":" + std::to_string(timestamp);
    // 简单 base64 编码（实际应使用 HMAC 签名，此处仅为演示）
    return base64Encode(raw);
}

bool UserHandler::validatePassword(const std::string& plain, const std::string& storedHash) {
    // 当前明文比较（因为 hashPassword 返回原字符串）
    // 后续应替换为 bcrypt::validate_password(plain, storedHash)
    return plain == storedHash;
}

std::string UserHandler::hashPassword(const std::string& plain) {
    // 当前直接返回明文（不安全！）
    // 后续应使用 bcrypt::generate_hash(plain, 12)
    return plain;
}