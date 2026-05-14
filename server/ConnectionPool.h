#pragma once

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>

/**
 * @brief PostgreSQL 连接池
 *
 * 管理一组 pqxx::connection 实例，支持多线程并发获取。
 * getConnection() 返回 RAII 守卫对象，析构时自动将连接归还池中。
 * 池满时调用方阻塞等待，直到有连接被归还。
 */
class ConnectionPool {
public:
    /**
     * @brief RAII 连接守卫
     *
     * 通过 operator* / operator-> 直接操作底层 pqxx::connection。
     * 不可拷贝，仅可移动。析构时自动归还连接到池。
     */
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

    /**
     * @brief 构造连接池
     * @param connStr  PostgreSQL 连接字符串
     * @param maxSize  最大并发连接数，默认 10
     */
    ConnectionPool(const std::string& connStr, int maxSize = 10);
    ~ConnectionPool();

    /**
     * @brief 从池中获取一个可用连接
     *
     * 优先复用空闲连接；无空闲且未达上限时创建新连接；
     * 已达上限则阻塞直到有连接归还。
     *
     * @return RAII 连接守卫，离开作用域时自动归还
     */
    ConnectionGuard getConnection();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

private:
    void returnConnection(std::unique_ptr<pqxx::connection> conn);
    std::unique_ptr<pqxx::connection> createConnection();

    std::string connStr_;
    int maxSize_;
    int activeCount_ = 0;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::unique_ptr<pqxx::connection>> pool_;
};
