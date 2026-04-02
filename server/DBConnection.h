#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>

class DBConnection
{
public:
    DBConnection(const std::string& connStr);
    bool connect();
    pqxx::connection& getConn();
    ~DBConnection() = default;  // unique_ptr 自动释放

    // 禁止拷贝
    DBConnection(const DBConnection&) = delete;
    DBConnection& operator=(const DBConnection&) = delete;
    // 允许移动（如果需要）
    DBConnection(DBConnection&&) = default;
    DBConnection& operator=(DBConnection&&) = default;

private:
    std::string connStr_;
    std::unique_ptr<pqxx::connection> conn_;
};