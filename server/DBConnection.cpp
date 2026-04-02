#include "DBConnection.h"
#include <iostream>

DBConnection::DBConnection(const std::string& connStr) : connStr_(connStr), conn_(nullptr) {}

bool DBConnection::connect()
{
    try {
        conn_ = std::make_unique<pqxx::connection>(connStr_);
        if (conn_->is_open()) {
            std::cout << "Connected to database successfully.\n";
            return true;
        } else {
            std::cout << "Failed to connect to database.\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

pqxx::connection& DBConnection::getConn()
{
    return *conn_;
}