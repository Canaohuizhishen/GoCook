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
    // ⭐ RAII 守卫：构造时从池里"借"连接，析构时自动"还"
    //   上层（Repository）只持有这个对象，用 *guard / guard-> 拿 pqxx::connection
    class ConnectionGuard {
    public:
        // 移动语义（getConnection 按值 return 时必须）：接管别人的连接，自己不留
        ConnectionGuard(ConnectionGuard&& other) noexcept
            : conn_(other.conn_), pool_(other.pool_), raw_(std::move(other.raw_)) {
            other.conn_ = nullptr;
            other.pool_ = nullptr;
        }

        ConnectionGuard& operator=(ConnectionGuard&& other) noexcept {
            if (this != &other) {
                if (raw_ && pool_) pool_->returnConnection(std::move(raw_));   // 先还掉自己手里的
                conn_ = other.conn_;
                pool_ = other.pool_;
                raw_ = std::move(other.raw_);
                other.conn_ = nullptr;
                other.pool_ = nullptr;
            }
            return *this;
        }

        ~ConnectionGuard() {
            if (raw_ && pool_) pool_->returnConnection(std::move(raw_));   // 析构必然归还
        }

        ConnectionGuard(const ConnectionGuard&) = delete;        // 禁止拷贝（一个连接不能被两份持有）
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

    /// 借连接：池里有空闲连接 → 取出复用；池空且没满 → 新建（懒创建）；
    /// 池空且满（存活连接数到上限）→ 阻塞等待归还，默认超时 5 秒，超时抛 ServiceException
    ConnectionGuard getConnection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    ConnectionPool(const ConnectionPool&) = delete;        // 禁止拷贝（连接池是全局唯一资源，不能复制）
    ConnectionPool& operator=(const ConnectionPool&) = delete;

private:
    void returnConnection(std::unique_ptr<pqxx::connection> conn);   // 归还：放回空闲队列 + 唤醒等待者
    std::unique_ptr<pqxx::connection> createConnection();            // 新建一条 libpq 连接（TCP 握手 + 认证）
    bool isConnectionAlive(pqxx::connection& conn);                  // 探活：SELECT 1

    std::string connStr_;                                            // 连接串（dbname=... user=... password=...）
    int maxSize_;                                                    // 池大小上限 = 数据库并发上限
    int activeCount_ = 0;                // 存活连接总数 = 池内空闲 + 借出中；归还不减、废弃死连接才减，恒 ≤ maxSize_

    mutable std::mutex mutex_;           // 保护线程共享数据结构 activeCount_ 和 pool_
    std::condition_variable cv_;         // 让"没借到连接的线程"安全地睡觉（阻塞），直到"归还连接的线程"把它叫醒
    std::deque<std::unique_ptr<pqxx::connection>> pool_;   // 空闲连接队列
};
