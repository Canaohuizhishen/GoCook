#include "ConnectionPool.h"
#include "common/Logger.h"
#include <gocook/IServices.h>
#include <stdexcept>

ConnectionPool::ConnectionPool(const std::string& connStr, int maxSize)
    : connStr_(connStr), maxSize_(maxSize) {}

ConnectionPool::~ConnectionPool() {
    std::unique_lock lock(mutex_);
    pool_.clear();
}

bool ConnectionPool::isConnectionAlive(pqxx::connection& conn) {
    if (!conn.is_open()) return false;
    try {
        pqxx::work txn(conn);
        txn.exec("SELECT 1");
        txn.commit();
        return true;
    } catch (...) {
        return false;
    }
}

std::unique_ptr<pqxx::connection> ConnectionPool::createConnection() {
    auto conn = std::make_unique<pqxx::connection>(connStr_);
    if (!conn->is_open()) {
        throw std::runtime_error("Failed to open database connection");
    }
    LOG_INFO("Database connection created (active=%d)", activeCount_ + 1);
    return conn;
}

ConnectionPool::ConnectionGuard ConnectionPool::getConnection(
    std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);

    while (pool_.empty() && activeCount_ >= maxSize_) {
        if (cv_.wait_for(lock, timeout) == std::cv_status::timeout) {
            throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
        }
    }

    if (!pool_.empty()) {
        auto raw = std::move(pool_.front());
        pool_.pop_front();
        pqxx::connection* ptr = raw.get();
        if (!isConnectionAlive(*ptr)) {
            LOG_WARN("Stale connection detected, discarding");
            ptr = nullptr;
            raw.reset();
            --activeCount_;
            if (activeCount_ < maxSize_) {
                raw = createConnection();
                ptr = raw.get();
                ++activeCount_;
            } else {
                throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
            }
        }
        return ConnectionGuard(ptr, this, std::move(raw));
    }

    auto raw = createConnection();
    pqxx::connection* ptr = raw.get();
    ++activeCount_;
    return ConnectionGuard(ptr, this, std::move(raw));
}

void ConnectionPool::returnConnection(std::unique_ptr<pqxx::connection> conn) {
    std::unique_lock lock(mutex_);
    if (!isConnectionAlive(*conn)) {
        LOG_WARN("Connection dead on return, discarding");
        --activeCount_;
    } else {
        pool_.push_back(std::move(conn));
    }
    if (activeCount_ < maxSize_) {
        cv_.notify_one();
    }
}
