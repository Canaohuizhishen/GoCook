#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

struct Config {
    std::string host = "0.0.0.0";
    int port = 8080;
    std::string dbConnString;
    std::string jwtSecret;
    std::string tlsCertPath;
    std::string tlsKeyPath;
    int dbPoolSize = 10;
    std::string logLevel = "info";

    static Config load() {
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
            // chdir to the directory containing .env so all relative paths resolve consistently
            auto dir = std::filesystem::absolute(envPath).parent_path();
            std::filesystem::current_path(dir);
        }
        if (!loaded) {
            // .env 不存在也没关系，调用方可能通过环境变量设了
        }
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
            throw std::runtime_error("GOCOOK_JWT_SECRET is required. Set it via env or .env file.(or by running server/setup.sh)");
        }
        if (cfg.dbConnString.empty()) {
            throw std::runtime_error("GOCOOK_DB_CONN_STRING is required. Set it via env or .env file.(or by running server/setup.sh)");
        }
        return cfg;
    }

private:
    static bool fileExists(const std::string& path) {
        std::ifstream f(path);
        return f.is_open();
    }

    static std::string getEnv(const std::string& key, const std::string& defaultValue) {
        const char* val = std::getenv(key.c_str());
        return val ? std::string(val) : defaultValue;
    }

    static int getEnvInt(const std::string& key, int defaultValue) {
        const char* val = std::getenv(key.c_str());
        if (!val) return defaultValue;
        try { return std::stoi(val); } catch (...) { return defaultValue; }
    }

    static void loadEnvFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            auto eq = trimmed.find('=');
            if (eq == std::string::npos) continue;
            auto key = trim(trimmed.substr(0, eq));
            auto value = trim(trimmed.substr(eq + 1));
            if (!value.empty() && (value.front() == '"' || value.front() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
            setenv(key.c_str(), value.c_str(), 1);
        }
    }

    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
};
