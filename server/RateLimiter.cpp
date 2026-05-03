#include "RateLimiter.h"
#include <algorithm>
#include <iostream>

using namespace std::chrono;

RateLimiter::RateLimiter(std::vector<Rule> rules, std::chrono::seconds cleanupInterval)
    : rules_(std::move(rules)), cleanupInterval_(cleanupInterval)
{
    // 启动后台清理线程
    cleanupThread_ = std::thread([this] { cleanupLoop(); });
}

RateLimiter::~RateLimiter()
{
    running_ = false;
    cv_.notify_all();                     // 立即唤醒清理线程，避免长时间等待
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();            // 几乎瞬间返回
    }
}

bool RateLimiter::isAllowed(const std::string& ip, const std::string& path)
{
    auto now = steady_clock::now();
    auto [windowSize, maxRequests] = matchRule(path);
    std::string key = ip + ":" + path;

    std::unique_lock lock(mutex_);
    auto& timestamps = records_[key];

    // 使用二分查找定位第一个仍然有效的时间戳，然后一次性移除所有过期记录
    auto windowStart = now - windowSize;
    auto it = std::upper_bound(timestamps.begin(), timestamps.end(), windowStart);
    if (it != timestamps.begin()) {
        timestamps.erase(timestamps.begin(), it);
    }

    if (static_cast<int>(timestamps.size()) >= maxRequests) {
        ++rejectedCount_; // 记录拒绝次数
        return false;     // 触发限流
    }

    timestamps.push_back(now);
    return true;
}

std::pair<std::chrono::seconds, int> RateLimiter::matchRule(const std::string& path) const
{
    // 按顺序匹配规则，返回第一个匹配的规则
    for (const auto& rule : rules_) {
        if (path.compare(0, rule.pathPrefix.size(), rule.pathPrefix) == 0) {
            return {rule.windowSize, rule.maxRequests};
        }
    }
    // 默认回退规则（每分钟 60 次）
    return {std::chrono::seconds(60), 60};
}

RateLimiter::Stats RateLimiter::getStats() const
{
    Stats s;
    std::shared_lock lock(mutex_);
    s.activeEntries = records_.size();
    lock.unlock();
    s.rejectedCount = rejectedCount_.load();
    return s;
}

void RateLimiter::cleanupLoop()
{
    while (running_) {
        {
            std::unique_lock lock(cvMutex_);
            // 可被析构函数通知提前结束的定时等待
            if (cv_.wait_for(lock, cleanupInterval_) == std::cv_status::no_timeout) {
                // 被唤醒，检查是否应该退出
                if (!running_) break;
            }
        }
        if (!running_) break;

        try {
            auto now = steady_clock::now();

            // 计算所有规则中最大的窗口，任何超出此窗口的记录均可安全删除
            auto maxWindowIt = std::max_element(rules_.begin(), rules_.end(),
                                                [](const Rule& a, const Rule& b) { return a.windowSize < b.windowSize; });
            auto threshold = now - (maxWindowIt != rules_.end() ? maxWindowIt->windowSize : seconds(60));

            // 阶段1：在共享锁内只收集需要检查的 key（尽可能快，不阻塞 isAllowed 读操作）
            std::vector<std::string> keysToCheck;
            {
                std::shared_lock lock(mutex_);
                keysToCheck.reserve(records_.size());
                for (const auto& pair : records_) {
                    keysToCheck.push_back(pair.first);
                }
            }

            // 阶段2：逐个 key 处理，每次加锁时间很短，不影响其他请求
            for (const auto& key : keysToCheck) {
                std::unique_lock lock(mutex_);
                auto it = records_.find(key);
                if (it != records_.end()) {
                    auto& timestamps = it->second;
                    // 使用二分查找定位过期记录的分界点
                    auto eraseIt = std::upper_bound(timestamps.begin(), timestamps.end(), threshold);
                    if (eraseIt != timestamps.begin()) {
                        timestamps.erase(timestamps.begin(), eraseIt);
                    }
                    if (timestamps.empty()) {
                        records_.erase(it);
                    }
                }
                // 锁在这里自动释放，给其他线程执行 isAllowed 的机会
            }
        } catch (const std::exception& e) {
            // 生产环境应接入日志系统，此处保留标准错误输出作为示意
            std::cerr << "RateLimiter cleanup error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "RateLimiter cleanup unknown error" << std::endl;
        }
    }
}