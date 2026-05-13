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
 * @brief 根据标准异常信息自动映射 HTTP 状态码并返回错误响应
 *        所有未匹配的异常均按 500 处理，并隐藏内部细节
 * @param e   捕获的异常
 * @param res  响应对象
 */
inline void handleStandardException(const std::exception &e, httplib::Response &res) {
    std::string what = e.what();

    // 按关键字映射可预期的业务错误
    if (what.find("not found") != std::string::npos || what.find("不存在") != std::string::npos) {
        setErrorResponse(res, 404, "请求的资源不存在");
    } else if (what.find("already exists") != std::string::npos || what.find("已存在") != std::string::npos) {
        setErrorResponse(res, 409, "资源冲突，该内容已存在");
    } else if (what.find("invalid") != std::string::npos || what.find("无效") != std::string::npos) {
        setErrorResponse(res, 400, "请求参数错误");
    } else if (what.find("permission") != std::string::npos || what.find("无权限") != std::string::npos) {
        setErrorResponse(res, 403, "权限不足");
    } else if (what.find("token") != std::string::npos || what.find("认证") != std::string::npos) {
        setErrorResponse(res, 401, "身份验证失败");
    } else {
        // 其他未知错误一律返回 500，并隐藏内部细节
        setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
    }

    // 生产环境应在此记录详细日志
    // LOG_ERROR("Handler exception: %s", e.what());
}
