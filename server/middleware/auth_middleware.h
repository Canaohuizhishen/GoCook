#pragma once
#include <string>
#include <functional>
#include <jwt-cpp/jwt.h>
#include <gocook/DataModels.h>
#include <gocook/IUserRepository.h>

/// 认证中间件，统一处理 JWT Token 验证 + 会话版本校验（JWT 主动吊销）
class AuthMiddleware {
public:
    /// 构造函数：JWT 签名密钥 + 用户仓库（读取 token_version 做主动吊销校验）
    AuthMiddleware(const std::string& secret, gocook::repository::IUserRepository& userRepo)
        : secret_(secret), userRepo_(userRepo) {}

    /// 验证请求头中的 Bearer Token，返回解析后的用户信息
    /// @param auth_header Authorization 请求头的值
    /// @return TokenInfo，valid 为 true 表示验证通过
    gocook::models::TokenInfo authenticate(const std::string& auth_header) const;

private:
    std::string secret_;
    gocook::repository::IUserRepository& userRepo_;  ///< 会话版本（token_version）数据来源
};