#pragma once

#include <httplib/httplib.h>
#include <gocook/DataModels.h>
#include "../middleware/auth_middleware.h"
#include "ErrorHelper.h"

/**
 * @brief 统一的认证入口，供所有 Handler 方法调用
 *
 * 从 Authorization 头中提取并验证 Bearer Token。
 * 验证失败时自动设置 401 JSON 响应（含 Content-Type），
 * 调用方仅需检查返回值的 valid 字段即可决定是否提前 return。
 *
 * @param auth  认证中间件
 * @param req   HTTP 请求
 * @param res   HTTP 响应（验证失败时会被写入 401 错误体）
 * @return      解析后的 TokenInfo，valid 为 true 表示认证通过
 */
inline gocook::models::TokenInfo requireAuth(AuthMiddleware& auth, const httplib::Request& req, httplib::Response& res) {
    auto info = auth.authenticate(req.get_header_value("Authorization"));
    if (!info.valid) {
        setErrorResponse(res, 401, "无效的访问令牌");
    }
    return info;
}
