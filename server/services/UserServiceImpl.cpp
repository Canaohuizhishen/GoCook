#include "UserServiceImpl.h"
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include "bcrypt/crypt_blowfish.h"   // 基于 Blowfish 算法的安全密码哈希（Openwall bcrypt 实现）

// bcrypt 输出缓冲区安全大小（实际输出约 60 字节）
#define BCRYPT_OUTPUT_SIZE 128

using namespace gocook::models;
using namespace gocook::services;

std::string UserServiceImpl::generateToken(int userId, const std::string& username, const std::string& role) {
    // 设置 Token 过期时间为 7 天后
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 7);

    auto token = jwt::create()
                     .set_issuer("GoCook")                                   // 签发者
                     .set_type("JWS")                                        // 类型
                     .set_payload_claim("userId", jwt::claim(std::to_string(userId))) // 用户ID
                     .set_payload_claim("username", jwt::claim(username))   // 用户名
                     .set_payload_claim("role", jwt::claim(role))           // 用户角色
                     .set_issued_at(now)                                     // 签发时间
                     .set_expires_at(exp)                                    // 过期时间
                     .sign(jwt::algorithm::hs256{jwt_secret});               // 使用 HMAC-SHA256 签名

    return token;
}

// 使用 crypt_blowfish 生成盐和哈希
std::string UserServiceImpl::hashPassword(const std::string& plain) {
    // 1. 生成 16 字节随机盐值
    char random_bytes[16];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for (int i = 0; i < 16; ++i) {
        random_bytes[i] = static_cast<char>(distrib(gen));
    }

    // 2. 调用 _crypt_gensalt_blowfish_rn 生成标准格式的盐串，如 "$2a$10$..."
    char salt[BCRYPT_OUTPUT_SIZE];
    char *salt_result = _crypt_gensalt_blowfish_rn("$2a$", 10, random_bytes, 16, salt, sizeof(salt));
    if (!salt_result) {
        throw ServiceException("Failed to generate password salt");
    }

    // 3. 用 _crypt_blowfish_rn 计算哈希
    char hash[BCRYPT_OUTPUT_SIZE];
    char *hash_result = _crypt_blowfish_rn(plain.c_str(), salt, hash, sizeof(hash));
    if (!hash_result) {
        throw ServiceException("Failed to hash password");
    }

    return std::string(hash_result);
}

bool UserServiceImpl::validatePassword(const std::string& plain, const std::string& hash) {
    char output[BCRYPT_OUTPUT_SIZE];
    char *result = _crypt_blowfish_rn(plain.c_str(), hash.c_str(), output, sizeof(output));
    if (!result) {
        return false;
    }
    // 常量时间比较，防止时序攻击
    int diff = 0;
    for (size_t i = 0; i < hash.size(); ++i) {
        diff |= (static_cast<unsigned char>(result[i]) ^ static_cast<unsigned char>(hash[i]));
    }
    return diff == 0;
}

// ------------------ IUserService 接口实现 ------------------

void UserServiceImpl::registerUser(const RegisterRequest& request) {
    try {
        pqxx::work txn(db_.getConn());

        pqxx::result check = txn.exec_params(
            "SELECT id FROM users WHERE username = $1", request.username);
        if (!check.empty()) {
            throw ServiceException("Username already exists");
        }

        std::string hashed = hashPassword(request.password);
        txn.exec_params(
            "INSERT INTO users (username, password_hash, email) VALUES ($1, $2, $3)",
            request.username, hashed, request.email);
        txn.commit();
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}

LoginResponse UserServiceImpl::login(const LoginRequest& request) {
    try {
        pqxx::work txn(db_.getConn());
        pqxx::result result = txn.exec_params(
            "SELECT id, password_hash, role FROM users WHERE username = $1",
            request.username);

        if (result.empty()) {
            throw ServiceException("Invalid username or password");
        }

        int userId = result[0]["id"].as<int>();
        std::string storedHash = result[0]["password_hash"].as<std::string>();
        std::string role = result[0]["role"].as<std::string>();   // 读取用户角色

        if (!validatePassword(request.password, storedHash)) {
            throw ServiceException("Invalid username or password");
        }

        LoginResponse resp;
        resp.user_id = userId;
        resp.username = request.username;
        resp.token = generateToken(userId, request.username, role);
        return resp;
    } catch (const std::exception& e) {
        throw ServiceException(std::string("Database error: ") + e.what());
    }
}