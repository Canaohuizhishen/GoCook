#include "auth_middleware.h"
#include "../common/Logger.h"
#include <chrono>   // 用于检查 Token 过期时间

using namespace gocook::models;

TokenInfo AuthMiddleware::authenticate(const std::string& auth_header) const {
    TokenInfo info;  // valid 默认为 false

    // 1. 检查 Authorization 头是否存在，且以 "Bearer " 开头
    if (auth_header.empty() || auth_header.find("Bearer ") != 0) {
        return info;
    }

    // 2. 提取 Token 字符串（去掉 "Bearer " 前缀）
    std::string token = auth_header.substr(7);

    try {
        // 3. 解码 JWT（不验证签名，仅解析）
        auto decoded = jwt::decode(token);

        // 4. 验证签名、算法和签发者
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{secret_})
                            .with_issuer("GoCook");
        verifier.verify(decoded);

        // 5. 显式检查过期时间（jwt-cpp 内部已验证，此处防御性再查）
        if (decoded.has_expires_at()) {
            auto exp_time = decoded.get_expires_at();
            if (std::chrono::system_clock::now() > exp_time) {
                throw std::runtime_error("令牌已过期");
            }
        } else {
            // 没有 exp 字段的 Token 视为非法
            throw std::runtime_error("令牌缺少过期时间");
        }

        // 6. 提取负荷中的用户信息
        info.userId = std::stoi(decoded.get_payload_claim("userId").as_string());
        info.username = decoded.get_payload_claim("username").as_string();
        info.role = decoded.get_payload_claim("role").as_string();   // 提取角色

        // 7. 会话版本校验（JWT 主动吊销）：与 users.token_version 比对。
        //    - ver 缺失按 0（兼容部署前签发的旧格式令牌，改密前仍有效）
        //    - 用户不存在 / 版本不符 → 无效；查库异常落入下方 catch（fail-closed，不放行）
        int issuedVersion = 0;
        if (decoded.has_payload_claim("ver")) {
            issuedVersion = std::stoi(decoded.get_payload_claim("ver").as_string());
        }
        const auto currentVersion = userRepo_.getTokenVersion(info.userId);
        if (!currentVersion.has_value() || currentVersion.value() != issuedVersion) {
            throw std::runtime_error("令牌已被吊销（会话版本不匹配）");
        }

        info.valid = true;

    } catch (const std::exception& e) {
        // 验证失败（签名错误、过期、格式不对等）
        LOG_WARN("JWT 身份验证失败：%s", e.what());
        info.valid = false;
    }

    return info;
}