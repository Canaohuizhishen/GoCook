#pragma once

#include <nlohmann/json.hpp>
#include <gocook/IServices.h>
#include <string>
#include <regex>
#include <cstdint>
#include <limits>

/// 统一输入校验族（validate* 请求校验函数）：失败即抛 ServiceException(400, 精确文案)，由 ErrorHelper 统一映射为 JSON 400 响应；成功返回 true。
/// 谓词辅助（isValidEmail / isIntInRange）不抛异常、返回 bool，供校验函数内部使用。

namespace Validation {

using json = nlohmann::json;

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

/// 整数范围判定：JSON 值是否为整数且落在 int 可表示范围内。
/// is_number_integer() 在 nlohmann 中同时覆盖无符号整数；越界值经 get<int>() 可能静默回绕，
/// 因此在形状校验阶段显式判定，保证后续 get<int>() / get<vector<int>>() 的提取安全。
inline bool isIntInRange(const json& v) {
    if (v.is_number_unsigned())
        return v.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    if (v.is_number_integer()) {
        const std::int64_t n = v.get<std::int64_t>();
        return n >= std::numeric_limits<int>::min() && n <= std::numeric_limits<int>::max();
    }
    return false;
}

/// 收藏 ID 列表字段（3.5 批量更新 / 3.6 批量删除共用）：
/// favorite_ids 必填、必须为数组、元素必须为 int 范围内的整数。
/// 空列表不在本层判定——3.5 / 3.6 均由服务层统一收口为 400「favorite_ids 不能为空」，这里只管 JSON 形状。
inline bool validateFavoriteIdsField(const json& j) {
    if (!j.contains("favorite_ids"))
        throw gocook::services::ServiceException("缺少 favorite_ids 字段", 400);
    if (!j["favorite_ids"].is_array())
        throw gocook::services::ServiceException("favorite_ids 必须为整数数组", 400);
    for (const auto& v : j["favorite_ids"]) {
        if (!isIntInRange(v))
            throw gocook::services::ServiceException("favorite_ids 必须为整数数组", 400);
    }
    return true;
}

/// 批量更新收藏（3.5）：favorite_ids 必填；group_id / is_public 可选，但出现即必须类型正确。
/// 空列表 / 缺更新字段的业务校验在服务层（同为 400）。
inline bool validateBatchUpdateFavoritesRequest(const json& j) {
    validateFavoriteIdsField(j);
    if (j.contains("group_id") && !isIntInRange(j["group_id"]))
        throw gocook::services::ServiceException("group_id 必须为整数", 400);
    if (j.contains("is_public") && !j["is_public"].is_boolean())
        throw gocook::services::ServiceException("is_public 必须为布尔值", 400);
    return true;
}

/// 批量删除收藏（3.6）：favorite_ids 必填；空列表由服务层收口为 400「favorite_ids 不能为空」（与 3.5 对称）。
inline bool validateBatchDeleteFavoritesRequest(const json& j) {
    validateFavoriteIdsField(j);
    return true;
}

} // namespace Validation
