#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>

class ConnectionPool {
public:
    class ConnectionGuard {
    public:
        ConnectionGuard(ConnectionGuard&& other) noexcept
            : conn_(other.conn_), pool_(other.pool_), raw_(std::move(other.raw_)) {
            other.conn_ = nullptr;
            other.pool_ = nullptr;
        }

        ConnectionGuard& operator=(ConnectionGuard&& other) noexcept {
            if (this != &other) {
                if (raw_ && pool_) pool_->returnConnection(std::move(raw_));
                conn_ = other.conn_;
                pool_ = other.pool_;
                raw_ = std::move(other.raw_);
                other.conn_ = nullptr;
                other.pool_ = nullptr;
            }
            return *this;
        }

        ~ConnectionGuard() {
            if (raw_ && pool_) pool_->returnConnection(std::move(raw_));
        }

        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;

        pqxx::connection& operator*() const { return *conn_; }
        pqxx::connection* operator->() const { return conn_; }

    private:
        friend class ConnectionPool;
        ConnectionGuard(pqxx::connection* conn, ConnectionPool* pool,
                        std::unique_ptr<pqxx::connection> raw)
            : conn_(conn), pool_(pool), raw_(std::move(raw)) {}

        pqxx::connection* conn_ = nullptr;
        ConnectionPool* pool_ = nullptr;
        std::unique_ptr<pqxx::connection> raw_;
    };

    ConnectionPool(const std::string& connStr, int maxSize = 10);
    ~ConnectionPool();

    /// 获取连接，默认超时 5 秒，超时抛出 ServiceException
    ConnectionGuard getConnection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

private:
    void returnConnection(std::unique_ptr<pqxx::connection> conn);
    std::unique_ptr<pqxx::connection> createConnection();
    bool isConnectionAlive(pqxx::connection& conn);

    std::string connStr_;
    int maxSize_;
    int activeCount_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::unique_ptr<pqxx::connection>> pool_;
};
