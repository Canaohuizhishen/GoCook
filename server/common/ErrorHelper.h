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
 * @brief 将 Handler 捕获的异常映射为 HTTP 错误响应：
 *        · ServiceException → 按其中 statusCode 映射（400/401/403/404/409/503 等）
 *        · nlohmann::json::exception → 按可回溯来源分类：
 *          – 请求体解析/字段读取（parse_error / out_of_range / 非 316 的 type_error 等）→ 400「请求体格式错误」：
 *            异常出自 req.body 的 json::parse 或字段提取（at/get/value），属客户端错误；
 *          – 序列化层非法 UTF-8（type_error.316，vendored nlohmann 3.12.0 中该 id 仅出自 serializer::dump_escaped）
 *            → 500：parse 阶段强制校验 UTF-8，请求体路径不可能产生 316；它只可能来自响应侧字符串——
 *            字面量、解析自请求、PostgreSQL 文本（UTF-8 编码）与服务端生成的文件名本应全部合法 UTF-8，
 *            该不变量一旦被破坏即服务端故障，不得伪装成 400。
 *          Repository 层的 JSON 异常已被 executeDb 统一翻译为 ServiceException，不受此分支影响。
 *        · 其他异常 → 500（不透传内部错误信息）
 * @param e   捕获的异常
 * @param res  响应对象
 */
inline void handleStandardException(const std::exception &e, httplib::Response &res) {
    // 运行时向下转型：Handler 收到的 e 是 std::exception 基类引用，实际传入的可能是子类 ServiceException。
    // 转型成功 → 业务异常，取其携带的 HTTP 状态码；转出 nullptr → 先按下方规则分类 JSON 异常，其余按
    // 系统底层异常处理（500），绝不把内部错误信息暴露给客户端。
    auto* se = dynamic_cast<const gocook::services::ServiceException*>(&e);
    if (!se) {
        auto* je = dynamic_cast<const nlohmann::json::exception*>(&e);
        if (je && je->id == 316) {
            // 316 = 序列化层非法 UTF-8（dump() 严格模式）：请求体路径到不了这里（parse 强制 UTF-8，
            // 非法输入在解析层即抛 parse_error）；能抛 316 的只有响应侧字符串，即服务端数据不变量
            // 被破坏（详见函数头注释）→ 500 并留痕。
            LOG_ERROR("响应序列化失败（非法 UTF-8，服务端数据不变量被破坏）：%s", e.what());
            setErrorResponse(res, 500, "服务器内部错误，请稍后重试");
            return;
        }
        if (je) {
            LOG_WARN("请求体 JSON 错误：%s", e.what());
            setErrorResponse(res, 400, "请求体格式错误");
            return;
        }
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
        // 503（连接池饱和等瞬时故障）：原样直通——客户端据此识别"服务器繁忙"并自动重试
        case 503: msg = se->what();            LOG_WARN("503: %s", se->what()); break;
        default:  msg = "服务器内部错误，请稍后重试"; code = 500;
                  LOG_ERROR("未识别的业务异常：%s（状态码 %d）", se->what(), code); break;
    }
    setErrorResponse(res, code, msg);
}
