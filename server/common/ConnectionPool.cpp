#include "ConnectionPool.h"
#include "Logger.h"              // LOG_WARN/LOG_INFO/LOG_DEBUG
#include <gocook/IServices.h>    // ServiceException 定义在这（双端共享契约）
#include <stdexcept>

ConnectionPool::ConnectionPool(const std::string& connStr, int maxSize)
    : connStr_(connStr), maxSize_(maxSize) {}

ConnectionPool::~ConnectionPool() {
    std::unique_lock lock(mutex_);
    pool_.clear();               // 空闲连接全部析构关闭
}

bool ConnectionPool::isConnectionAlive(pqxx::connection& conn) {
    if (!conn.is_open()) return false;
    try {
        pqxx::work txn(conn);
        txn.exec("SELECT 1");
        txn.abort();
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

    // ① 池空 + 存活连接数到上限（满池）→ 阻塞等待归还；等 timeout 还等不到 → 503（绝不无限挂起）
    while (pool_.empty() && activeCount_ >= maxSize_) {
        if (cv_.wait_for(lock, timeout) == std::cv_status::timeout) {
            throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
        }
    }

    if (!pool_.empty()) {        // ② 有闲置连接 → 取出复用
        auto raw = std::move(pool_.front());
        pool_.pop_front();
        pqxx::connection* ptr = raw.get();
        if (!ptr->is_open()) {   // ③ 死连接检测：数据库重启后连接已断，丢弃重建
            LOG_WARN("Stale connection detected, discarding");
            ptr = nullptr;
            raw.reset();         //    销毁死连接 → 存活总数 -1
            --activeCount_;
            if (activeCount_ < maxSize_) {
                raw = createConnection();   //    重建一条（存活总数回到上限内）
                ptr = raw.get();
                ++activeCount_;
            } else {
                throw gocook::services::ServiceException("系统繁忙，请稍后重试", 503);
            }
        }
        return ConnectionGuard(ptr, this, std::move(raw));
    }

    // ④ 池空且没满 → 新建（懒创建：第一个请求才建，慢慢涨到 maxSize）
    auto raw = createConnection();
    pqxx::connection* ptr = raw.get();
    ++activeCount_;
    return ConnectionGuard(ptr, this, std::move(raw));
}

void ConnectionPool::returnConnection(std::unique_ptr<pqxx::connection> conn) {
    std::unique_lock lock(mutex_);
    pool_.push_back(std::move(conn));
    // 归还使池由空变非空 → 唤醒一个正在等连接的线程（等待者只在"池空且满"时存在）；
    // 若此刻没有等待者，notify_one 是一次 O(1) 的无害空唤醒
    cv_.notify_one();
}
