#pragma once

#include <nlohmann/json.hpp>
#include <gocook/IServices.h>
#include <string>
#include <regex>

/// 统一输入校验。返回 true 表示通过；返回 false 时 error 已设置好 HTTP 响应。

namespace Validation {

using json = nlohmann::json;

/// 检查 JSON 是否包含全部指定字段，缺失时通过 missing 返回第一个缺失字段名
inline bool hasFields(const json& j, const std::vector<std::string>& keys,
                      std::string& missing) {
    for (const auto& k : keys) {
        if (!j.contains(k)) { missing = k; return false; }
    }
    return true;
}

/// 简单邮箱格式校验（正则）（它无法匹配 IP 地址形式的邮箱（如 user@[192.168.1.1]），
/// 也无法匹配包含中文的国际化域名（IDN）（如 用户@例子.中国），
/// 另外，它也没校验顶级域名的合法性（com 和 xyz 都过，123 也能过））
inline bool isValidEmail(const std::string& email) {
    static const std::regex re(R"(^[^\s@]+@[^\s@]+\.[^\s@]{2,}$)");
    return std::regex_match(email, re);
}

/// 注册请求：username 1-30 字符，password ≥ 6 字符，email 格式
inline bool validateRegisterRequest(const json& j) {
    if (!j.contains("username") || !j["username"].is_string() ||
        j["username"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少用户名", 400);
    std::string u = j["username"];
    if (u.size() > 30)
        throw gocook::services::ServiceException("用户名不能超过30个字符", 400);
    if (!j.contains("password") || !j["password"].is_string() ||
        j["password"].get<std::string>().size() < 6)
        throw gocook::services::ServiceException("密码不能少于6个字符", 400);
    if (!j.contains("email") || !j["email"].is_string() ||
        !isValidEmail(j["email"]))
        throw gocook::services::ServiceException("邮箱格式无效", 400);
    return true;
}

/// 登录请求：username、password 必填
inline bool validateLoginRequest(const json& j) {
    if (!j.contains("username") || !j["username"].is_string() ||
        j["username"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少用户名", 400);
    if (!j.contains("password") || !j["password"].is_string() ||
        j["password"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少密码", 400);
    return true;
}

/// 忘记密码：username + email 均必填
inline bool validateForgotPasswordRequest(const json& j) {
    if (!j.contains("username") || !j["username"].is_string() ||
        j["username"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少用户名", 400);
    if (!j.contains("email") || !j["email"].is_string() ||
        !isValidEmail(j["email"]))
        throw gocook::services::ServiceException("邮箱格式无效", 400);
    return true;
}

/// 注册验证（两段式注册第二步）：email 与 token 均必填
inline bool validateVerifyRegistrationRequest(const json& j) {
    if (!j.contains("email") || !j["email"].is_string() ||
        !isValidEmail(j["email"]))
        throw gocook::services::ServiceException("邮箱格式无效", 400);
    if (!j.contains("token") || !j["token"].is_string() ||
        j["token"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少验证码", 400);
    return true;
}

/// 重置密码：token 和新密码必填，密码 ≥ 6 字符，对应 “忘记密码” 流程。
/// 调用时，用户处于未登录状态。系统不信任用户知道旧密码，而是通过“邮箱+用户名”双验证后，
/// 颁发一个一次性重置令牌（token）来证明身份。
inline bool validateResetPasswordRequest(const json& j) {
    if (!j.contains("token") || !j["token"].is_string() ||
        j["token"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少重置令牌", 400);
    if (!j.contains("new_password") || !j["new_password"].is_string() ||
        j["new_password"].get<std::string>().size() < 6)
        throw gocook::services::ServiceException("新密码不能少于6个字符", 400);
    return true;
}

/// 库存项：ingredient_name、quantity、unit 必填
inline bool validateInventoryRequest(const json& j) {
    if (!j.contains("ingredient_name") || !j["ingredient_name"].is_string() ||
        j["ingredient_name"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少食材名称", 400);
    if (!j.contains("quantity") || !j["quantity"].is_number())
        throw gocook::services::ServiceException("缺少数量或格式无效", 400);
    if (!j.contains("unit") || !j["unit"].is_string() ||
        j["unit"].get<std::string>().empty())
        throw gocook::services::ServiceException("缺少单位", 400);
    return true;
}

/// 评分：1-5 之间
inline bool validateRating(int rating) {
    if (rating < 1 || rating > 5)
        throw gocook::services::ServiceException("评分必须在1到5之间", 400);
    return true;
}

/// 修改密码：新旧密码必填，新密码 ≥ 6 字符，对应 “账号安全” 流程。
/// 调用时，用户处于已登录状态（JWT Token 有效）。系统需要验证用户知道当前密码（current_password），
/// 以防止别人趁你电脑未锁屏时恶意篡改密码。
inline bool validateChangePasswordRequest(const json& j) {
    if (!j.contains("current_password") || !j["current_password"].is_string())
        throw gocook::services::ServiceException("缺少当前密码", 400);
    if (!j.contains("new_password") || !j["new_password"].is_string() ||
        j["new_password"].get<std::string>().size() < 6)
        throw gocook::services::ServiceException("新密码不能少于6个字符", 400);
    return true;
}

} // namespace Validation
