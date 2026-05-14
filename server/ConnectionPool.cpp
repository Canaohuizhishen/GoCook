#include "ConnectionPool.h"
#include <iostream>
#include <stdexcept>

ConnectionPool::ConnectionPool(const std::string& connStr, int maxSize)
    : connStr_(connStr), maxSize_(maxSize) {}

ConnectionPool::~ConnectionPool() {
    std::unique_lock lock(mutex_);
    pool_.clear();
}

std::unique_ptr<pqxx::connection> ConnectionPool::createConnection() {
    auto conn = std::make_unique<pqxx::connection>(connStr_);
    if (!conn->is_open()) {
        throw std::runtime_error("Failed to open database connection");
    }
    return conn;
}

ConnectionPool::ConnectionGuard ConnectionPool::getConnection() {
    std::unique_lock lock(mutex_);

    // 无空闲连接且已达上限，阻塞等待
    while (pool_.empty() && activeCount_ >= maxSize_) {
        cv_.wait(lock);
    }

    if (!pool_.empty()) {
        // 复用池中已有连接，取出前做活性检查
        auto raw = std::move(pool_.front());
        pool_.pop_front();
        pqxx::connection* ptr = raw.get();
        if (!ptr->is_open()) {
            // 连接已断开，丢弃并尝试重建
            ptr = nullptr;
            raw.reset();
            --activeCount_;
            if (activeCount_ < maxSize_) {
                raw = createConnection();
                ptr = raw.get();
                ++activeCount_;
            } else {
                throw std::runtime_error("Database connection pool exhausted and all connections dead");
            }
        }
        return ConnectionGuard(ptr, this, std::move(raw));
    }

    // 池空且未达上限，新建连接
    auto raw = createConnection();
    pqxx::connection* ptr = raw.get();
    ++activeCount_;
    return ConnectionGuard(ptr, this, std::move(raw));
}

void ConnectionPool::returnConnection(std::unique_ptr<pqxx::connection> conn) {
    std::unique_lock lock(mutex_);
    if (conn->is_open()) {
        pool_.push_back(std::move(conn));
    } else {
        // 连接已断开则不再回收
        --activeCount_;
    }
    // 通知可能正在等待的线程
    if (activeCount_ < maxSize_) {
        cv_.notify_one();
    }
}
