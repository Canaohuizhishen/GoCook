#pragma once

#include <string>

struct TokenInfo {
    int userId;
    std::string username;
    bool valid;
};

// 验证 token，返回用户信息
TokenInfo verifyToken(const std::string& token);