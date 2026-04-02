#pragma once

#include "./third_party/httplib/httplib.h"
#include "DBConnection.h"
#include "./nlohmann/json.hpp"
#include <string>

using json = nlohmann::json;

class UserHandler {
public:
    explicit UserHandler(DBConnection& db);

    // 注册新用户
    void registerUser(const httplib::Request& req, httplib::Response& res);
    // 用户登录，返回 token
    void loginUser(const httplib::Request& req, httplib::Response& res);
    // 返回所有用户信息，公开测试接口（无需认证）
    void getUsersPublic(const httplib::Request& req, httplib::Response& res);

private:
    DBConnection& db_;

    // 生成简单 token（生产环境应使用 JWT 或随机字符串 + 服务端存储）
    std::string generateToken(int userId, const std::string& username);
    // 验证用户名密码（明文比较，后续应改为 bcrypt 验证）
    bool validatePassword(const std::string& plain, const std::string& storedHash);
    // 哈希密码（当前直接返回原字符串，后续替换为 bcrypt）
    std::string hashPassword(const std::string& plain);
};