#pragma once

#include <string>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <mutex>

/// 日志级别（从低到高；低于全局配置级别的日志不输出）
enum class LogLevel { Debug, Info, Warn, Error };

// 日志级别转定宽字符串（如 "INFO "），用于日志前缀
inline const char* logLevelName(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

// 字符串转日志级别（未知值回退为 Info）
inline LogLevel logLevelFromStr(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "warn")  return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Info;
}

// 全局日志互斥锁，保证多线程写 stderr 不交错
inline std::mutex gLogMutex;

// 写入一条日志：先做级别过滤，再输出"时间戳 [级别] 文件名:行号 消息"到 stderr
inline void logWrite(LogLevel lv, const char* file, int line, const std::string& msg) {
    static LogLevel gLevel = LogLevel::Info;
    static bool gInited = false;
    if (!gInited) {
        const char* env = std::getenv("GOCOOK_LOG_LEVEL");
        if (env) gLevel = logLevelFromStr(std::string(env));
        gInited = true;
    }
    if (lv < gLevel) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&time));

    const char* fname = file;
    const char* lastSlash = file;
    for (const char* p = file; *p; ++p)
        if (*p == '/' || *p == '\\') lastSlash = p + 1;

    std::lock_guard lock(gLogMutex);
    fprintf(stderr, "[%s.%03d] [%s] %s:%d  %s\n",
            timeBuf, static_cast<int>(ms.count()),
            logLevelName(lv), lastSlash, line, msg.c_str());
    fflush(stderr);
}

// printf 风格格式化后写日志（消息缓冲上限 2048 字节）
template<typename... Args>
inline void logWriteF(LogLevel lv, const char* file, int line,
                      const char* fmt, Args&&... args) {
    char buf[2048];
    int n = snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    if (n < 0) return;
    logWrite(lv, file, line, std::string(buf, std::min(static_cast<size_t>(n), sizeof(buf) - 1)));
}

// 输出 Debug 级别日志
#define LOG_DEBUG(...) logWriteF(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
// 输出 Info 级别日志
#define LOG_INFO(...)  logWriteF(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
// 输出 Warn 级别日志
#define LOG_WARN(...)  logWriteF(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
// 输出 Error 级别日志
#define LOG_ERROR(...) logWriteF(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
