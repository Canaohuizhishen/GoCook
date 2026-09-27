#pragma once

#include <algorithm>
#include <string>
#include <httplib/httplib.h>

/// 分页参数解析结果
struct PaginationParams {
    int page = 1;
    int size = 20;
};

/**
 * @brief 安全解析请求中的分页参数（page / size）
 *
 * 自动对 page 施加 >0 下限，对 size 施加 1～maxSize 钳位，
 * 非数字输入静默回退到默认值，调用处无需额外 try/catch。
 *
 * @param req          HTTP 请求对象
 * @param defaultSize  未传 size 时的默认值
 * @param maxSize      size 允许的最大值
 */
inline PaginationParams parsePagination(const httplib::Request& req, int defaultSize = 20, int maxSize = 100) {
    PaginationParams p{1, defaultSize};
    try {
        if (req.has_param("page")) {
            p.page = std::stoi(req.get_param_value("page"));
            if (p.page < 1) p.page = 1;
        }
        if (req.has_param("size")) {
            p.size = std::stoi(req.get_param_value("size"));
            if (p.size < 1) p.size = 1;
            if (p.size > maxSize) p.size = maxSize;
        }
    } catch (...) {
        // 非数字输入忽略，使用默认值
    }
    return p;
}

/**
 * @brief 安全解析请求中的整数参数
 *
 * 非数字或缺失时返回 defaultValue，避免 std::stoi 直接抛异常。
 */
inline int parseIntParam(const httplib::Request& req, const std::string& name, int defaultValue = 0) {
    try {
        if (req.has_param(name)) {
            return std::stoi(req.get_param_value(name));
        }
    } catch (...) {}
    return defaultValue;
}

/**
 * @brief 安全解析请求中的无符号整数参数（有效范围 0 ~ 4294967295）
 *
 * 仅接受纯数字写法；非数字 / 负号 / 空白等带符号写法 / 超出范围，
 * 一律静默返回 defaultValue（不抛异常）。
 * 注意：不能直接用 std::stoul——它对 "-1" 这类负值按回绕处理而不是报错，
 * 必须先做"纯数字"预检；超出 unsigned long long 的长数字则由 stoull 抛错兜住。
 */
inline unsigned int parseUIntParam(const httplib::Request& req, const std::string& name, unsigned int defaultValue = 0) {
    try {
        if (req.has_param(name)) {
            const std::string v = req.get_param_value(name);
            if (!v.empty() && v.find_first_not_of("0123456789") == std::string::npos) {
                const unsigned long long val = std::stoull(v);
                if (val <= 0xFFFFFFFFull) return static_cast<unsigned int>(val);
            }
        }
    } catch (...) {}
    return defaultValue;
}
