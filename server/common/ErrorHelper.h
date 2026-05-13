#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <httplib/httplib.h>

/**
 * @brief 设置 HTTP 错误响应
 * @param res   响应对象
 * @param status HTTP 状态码
 * @param message 错误描述（中文）
 */
inline void setErrorResponse(httplib::Response &res, int status, const std::string &message) {
    res.status = status;
    res.set_header("Content-Type", "application/json");
    nlohmann::json body;
    body["error"] = message;
    res.body = body.dump();
}

/**
 * @brief 根据 ServiceException 中的 statusCode 自动映射 HTTP 状态码并返回错误响应
 *        所有非 ServiceException 或未识别的异常均按 500 处理
 * @param e   捕获的异常
 * @param res  响应对象
 */
inline void handleStandardException(const std::exception &e, httplib::Response &res) {
    auto* se = dynamic_cast<const gocook::services::ServiceException*>(&e);
    if (!se) {
        setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
        return;
    }
    std::string msg;
    int code = se->statusCode();
    switch (code) {
        case 404: msg = "请求的资源不存在"; break;
        case 409: msg = se->what(); break;   // 原文：用户可见的具体原因
        case 400: msg = se->what(); break;   // 原文：具体参数错误
        case 403: msg = se->what(); break;   // 原文："仅可编辑未审核的菜谱"
        case 401: msg = "身份验证失败"; break;
        default:  msg = "服务器内部错误，请稍后重试"; code = 500; break;
    }
    setErrorResponse(res, code, msg);
}
