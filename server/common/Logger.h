#pragma once

#include <string>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <mutex>

enum class LogLevel { Debug, Info, Warn, Error };

inline const char* logLevelName(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

inline LogLevel logLevelFromStr(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "warn")  return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Info;
}

inline std::mutex gLogMutex;

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

template<typename... Args>
inline void logWriteF(LogLevel lv, const char* file, int line,
                      const char* fmt, Args&&... args) {
    char buf[2048];
    int n = snprintf(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    if (n < 0) return;
    logWrite(lv, file, line, std::string(buf, std::min(static_cast<size_t>(n), sizeof(buf) - 1)));
}

#define LOG_DEBUG(...) logWriteF(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logWriteF(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logWriteF(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logWriteF(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
