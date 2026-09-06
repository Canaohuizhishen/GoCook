#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

/**
 * @brief 服务器运行配置，从环境变量或 .env 文件加载。
 *
 * 支持 GOCOOK_* 系列环境变量（默认值见各字段）；加载 .env 文件后
 * 会 chdir 到其所在目录，保证所有相对路径一致解析。
 */
struct Config {
    std::string host = "0.0.0.0";  ///< 监听地址
    int port = 8080;               ///< 监听端口
    std::string dbConnString;      ///< PostgreSQL 连接串
    std::string jwtSecret;         ///< JWT 签名密钥（必填）
    std::string tlsCertPath;       ///< TLS 证书路径（可选）
    std::string tlsKeyPath;        ///< TLS 私钥路径（可选）
    int dbPoolSize = 10;           ///< 数据库连接池大小
    std::string logLevel = "info"; ///< 日志级别：debug/info/warn/error

    /**
     * @brief 从环境变量与 .env 文件加载配置。
     * @return 填充完成的配置对象
     * @throw std::runtime_error 缺少必填项 GOCOOK_JWT_SECRET 或
     *        GOCOOK_DB_CONN_STRING 时抛出
     */
    static Config load() {
        // 查找 .env 文件（四层候选）
        const char* candidates[] = {".env", "../../../.env", "../../.env", "../.env"};
        bool loaded = false;
        std::string envPath;
        for (auto p : candidates) {
            if (fileExists(p)) {
                envPath = p;
                loadEnvFile(p);
                loaded = true;
                break;
            }
        }
        if (!envPath.empty()) {
            // chdir 到 .env 所在目录，使所有相对路径都能一致地解析
            auto dir = std::filesystem::absolute(envPath).parent_path();
            std::filesystem::current_path(dir);
        }
        if (!loaded) {
            // .env 不存在也没关系，调用方可能通过环境变量设了
        }

        // 读取环境变量（无或类型转换(std::stoi)失败使用默认值）
        Config cfg;
        cfg.host         = getEnv("GOCOOK_HOST",          "0.0.0.0");
        cfg.port         = getEnvInt("GOCOOK_PORT",        8080);
        cfg.dbConnString = getEnv("GOCOOK_DB_CONN_STRING", "");
        cfg.jwtSecret    = getEnv("GOCOOK_JWT_SECRET",     "");
        cfg.tlsCertPath  = getEnv("GOCOOK_TLS_CERT",       "");
        cfg.tlsKeyPath   = getEnv("GOCOOK_TLS_KEY",        "");
        cfg.dbPoolSize   = getEnvInt("GOCOOK_DB_POOL_SIZE", 10);
        cfg.logLevel     = getEnv("GOCOOK_LOG_LEVEL",      "info");

        if (cfg.jwtSecret.empty()) {
            throw std::runtime_error("缺少 GOCOOK_JWT_SECRET：请通过环境变量或 .env 文件设置（或运行 server/setup.sh）");
        }
        if (cfg.dbConnString.empty()) {
            throw std::runtime_error("缺少 GOCOOK_DB_CONN_STRING：请通过环境变量或 .env 文件设置（或运行 server/setup.sh）");
        }
        return cfg;
    }

private:
    // 判断文件是否存在
    static bool fileExists(const std::string& path) {
        std::ifstream f(path);
        return f.is_open();
    }

    // 读取环境变量，未设置时返回默认值
    static std::string getEnv(const std::string& key, const std::string& defaultValue) {
        const char* val = std::getenv(key.c_str());
        return val ? std::string(val) : defaultValue;
    }

    // 读取整数环境变量，未设置或非法输入时返回默认值
    static int getEnvInt(const std::string& key, int defaultValue) {
        const char* val = std::getenv(key.c_str());
        if (!val) return defaultValue;
        try { return std::stoi(val); } catch (...) { return defaultValue; }
    }

    // 加载 .env 文件（KEY=VALUE，支持引号）并写入进程环境变量
    static void loadEnvFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;           // 文件不存在就算了，允许用系统环境变量

        std::string line;
        while (std::getline(file, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;  // 跳过空行和注释

            auto eq = trimmed.find('=');
            if (eq == std::string::npos) continue;               // 没等号就不是合法配置行

            auto key = trim(trimmed.substr(0, eq));              // 取键名
            auto value = trim(trimmed.substr(eq + 1));           // 取值

            // 如果值被引号包着，去掉首尾引号
            if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }

            setenv(key.c_str(), value.c_str(), 1);               // 写入环境变量（覆盖同名的）
        }
    }

    // 去除字符串首尾空白字符
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};
