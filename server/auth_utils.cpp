#include "auth_utils.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

// 解码 base64（简单实现，与编码对应）
static std::string base64Decode(const std::string& encoded) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

TokenInfo verifyToken(const std::string& token) {
    TokenInfo info;
    info.valid = false;

    // 尝试 base64 解码
    std::string decoded;
    try {
        decoded = base64Decode(token);
    } catch (...) {
        return info;
    }

    // 格式: userId:username:timestamp
    std::vector<std::string> parts;
    std::stringstream ss(decoded);
    std::string part;
    while (std::getline(ss, part, ':')) {
        parts.push_back(part);
    }
    if (parts.size() != 3) return info;

    // 简单校验：userId 必须是数字，username 非空
    try {
        info.userId = std::stoi(parts[0]);
        info.username = parts[1];
        // 可选：检查时间戳是否过期（比如 7 天内），当前忽略
        info.valid = true;
    } catch (...) {
        info.valid = false;
    }
    return info;
}