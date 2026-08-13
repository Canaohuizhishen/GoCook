#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <condition_variable>

/**
 * @brief 生产级请求频率限制器
 *
 * 基于滑动窗口算法，支持为不同路径前缀配置独立的限制规则。
 * 线程安全，内置自动清理机制，具备可观测性接口，适合长期运行的在线服务。
 */
class RateLimiter {
public:
    /// 单条频率限制规则
    struct Rule {
        std::string pathPrefix;             ///< 路径前缀，用于匹配请求路径
        std::chrono::seconds windowSize;    ///< 时间窗口大小（秒）
        int maxRequests;                    ///< 窗口内允许的最大请求数
    };

    /// 运行状态快照（供监控使用）
    struct Stats {
        size_t activeEntries;    ///< 当前活跃的记录数
        size_t rejectedCount;   ///< 累计被拒绝的请求数
    };

    /**
     * @brief 构造函数
     * @param rules 频率限制规则列表，按添加顺序匹配，第一条匹配的规则生效
     * @param cleanupInterval 后台清理线程的运行间隔（默认 60 秒）
     * @param maxRecords 记录表最大条目数，超过时后台清理线程会驱逐最久远的条目（默认 10000）
     */
    explicit RateLimiter(std::vector<Rule> rules,
                         std::chrono::seconds cleanupInterval = std::chrono::seconds(60),
                         size_t maxRecords = 10000);

    ~RateLimiter();

    /// 禁止拷贝和移动
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /**
     * @brief 检查某个 IP 对指定路径的请求是否被允许
     * @param ip 客户端 IP 地址
     * @param path 请求路径
     * @return true 允许，false 触发限流
     */
    bool isAllowed(const std::string& ip, const std::string& path);

    /// 获取运行统计信息（用于监控）
    Stats getStats() const;

private:
    /// 匹配路径对应的规则（返回窗口大小和最大请求数）
    std::pair<std::chrono::seconds, int> matchRule(const std::string& path) const;

    /// 后台清理线程执行函数
    void cleanupLoop();

    /// 锁住整个记录表，用于读写操作。使用 mutable 修饰，使得统计类 const 成员函数也能安全加锁，不破坏对象的逻辑常量性。
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> records_;

    std::vector<Rule> rules_;
    std::chrono::seconds cleanupInterval_;
    size_t maxRecords_;          ///< 记录表容量上限，防止无界增长

    std::atomic<bool> running_{true};
    std::thread cleanupThread_;

    /// 可观测指标
    std::atomic<size_t> rejectedCount_{0};

    /// 用于优雅停止清理线程
    std::mutex cvMutex_;
    std::condition_variable cv_;
};