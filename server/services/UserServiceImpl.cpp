#include "UserServiceImpl.h"
#include <pqxx/pqxx>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

using namespace gocook::models;
using namespace gocook::services;

std::string UserServiceImpl::base64Encode(const std::string& input) {
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

std::string UserServiceImpl::generateToken(int userId, const std::string& username) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string raw = std::to_string(userId) + ":" + username + ":" + std::to_string(timestamp);
    return base64Encode(raw);
}

bool UserServiceImpl::validatePassword(const std::string& plain, const std::string& storedHash) {
    // 当前为明文比较（后续应替换为 bcrypt 验证）
    return plain == storedHash;
}

std::string UserServiceImpl::hashPassword(const std::string& plain) {
    // 当前直接返回明文（不安全，后续应替换为 bcrypt）
    return plain;
}

// ------------------ IUserService 接口实现 ------------------

void UserServiceImpl::registerUser(const RegisterRequest& request) {
    try {
        pqxx::work txn(db_.getConn());

        // 使用新式 exec 配合 quote，避免弃用 exec_params
        pqxx::result check = txn.exec(
            "SELECT id FROM users WHERE username = " + txn.quote(request.username));
        if (!check.empty()) {
            throw ServiceException("Username already exists");
        }

        std::string hashed = hashPassword(request.password);
        txn.exec(
            "INSERT INTO users (username, password_hash) VALUES (" +
            txn.quote(request.username) + ", " + txn.quote(hashed) + ")");
        txn.commit();
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}

LoginResponse UserServiceImpl::login(const LoginRequest& request) {
    try {
        pqxx::work txn(db_.getConn());
        pqxx::result result = txn.exec(
            "SELECT id, password_hash FROM users WHERE username = " + txn.quote(request.username));

        if (result.empty()) {
            throw ServiceException("Invalid username or password");
        }

        int userId = result[0]["id"].as<int>();
        std::string storedHash = result[0]["password_hash"].as<std::string>();

        if (!validatePassword(request.password, storedHash)) {
            throw ServiceException("Invalid username or password");
        }

        LoginResponse resp;
        resp.user_id = userId;
        resp.username = request.username;
        resp.token = generateToken(userId, request.username);
        return resp;
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}