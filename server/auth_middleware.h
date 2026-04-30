#pragma once
#include <string>
#include <functional>
#include <jwt-cpp/jwt.h>
#include <gocook/DataModels.h>

/// 认证中间件，统一处理 JWT Token 验证
class AuthMiddleware {
public:
    /// 构造函数，传入 JWT 签名密钥
    explicit AuthMiddleware(const std::string& secret) : secret_(secret) {}

    /// 验证请求头中的 Bearer Token，返回解析后的用户信息
    /// @param auth_header Authorization 请求头的值
    /// @return TokenInfo，valid 为 true 表示验证通过
    gocook::models::TokenInfo authenticate(const std::string& auth_header) const;

private:
    std::string secret_;
};