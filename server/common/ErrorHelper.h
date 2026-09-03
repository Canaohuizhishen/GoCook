#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <httplib/httplib.h>
#include "Logger.h"

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
    // 这一行是运行时向下转型：因为 Handler 收到的 e 是 std::exception 基类引用，但实际传入的可能是子类 ServiceException。
    // 我用 dynamic_cast 去试探它：如果转型成功，说明是业务异常，我可以拿到它携带的 HTTP 状态码；
    // 如果转出来是 nullptr，说明是系统底层异常（如内存或 SQL 错误），我就直接把它当 500 处理，绝不把内部错误信息暴露给客户端。
    auto* se = dynamic_cast<const gocook::services::ServiceException*>(&e);
    if (!se) {
        LOG_ERROR("未处理异常：%s", e.what());
        setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
        return;
    }
    std::string msg;
    int code = se->statusCode();
    switch (code) {
        case 404: msg = "请求的资源不存在";    LOG_WARN("404: %s", se->what()); break;
        case 409: msg = se->what();            LOG_WARN("409: %s", se->what()); break;
        case 400: msg = se->what();            LOG_WARN("400: %s", se->what()); break;
        case 403: msg = se->what();            LOG_WARN("403: %s", se->what()); break;
        case 401: msg = "身份验证失败";         LOG_WARN("401: %s", se->what()); break;
        case 501: msg = "功能暂未实现";         LOG_WARN("501: %s", se->what()); break;
        default:  msg = "服务器内部错误，请稍后重试"; code = 500;
                  LOG_ERROR("未识别的业务异常：%s（状态码 %d）", se->what(), code); break;
    }
    setErrorResponse(res, code, msg);
}
