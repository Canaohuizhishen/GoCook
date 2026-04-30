#include "UserServiceImpl.h"
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

using namespace gocook::models;
using namespace gocook::services;

std::string UserServiceImpl::generateToken(int userId, const std::string& username) {
    // 设置 Token 过期时间为 7 天后
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 7);

    auto token = jwt::create()
                     .set_issuer("GoCook")                                   // 签发者
                     .set_type("JWS")                                        // 类型
                     .set_payload_claim("userId", jwt::claim(std::to_string(userId))) // 用户ID
                     .set_payload_claim("username", jwt::claim(username))   // 用户名
                     .set_issued_at(now)                                     // 签发时间
                     .set_expires_at(exp)                                    // 过期时间
                     .sign(jwt::algorithm::hs256{jwt_secret});               // 使用 HMAC-SHA256 签名

    return token;
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