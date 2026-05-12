#include "auth_middleware.h"
#include <iostream>

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

        // 5. 提取负荷中的用户信息
        info.userId = std::stoi(decoded.get_payload_claim("userId").as_string());
        info.username = decoded.get_payload_claim("username").as_string();
        info.role = decoded.get_payload_claim("role").as_string();   // 提取角色
        info.valid = true;

    } catch (const std::exception& e) {
        // 验证失败（签名错误、过期、格式不对等）
        std::cerr << "JWT authentication failed: " << e.what() << std::endl;
        info.valid = false;
    }

    return info;
}