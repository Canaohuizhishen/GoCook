#include "RateLimiter.h"
#include <algorithm>
#include "common/Logger.h"

using namespace std::chrono;

RateLimiter::RateLimiter(std::vector<Rule> rules, std::chrono::seconds cleanupInterval, size_t maxRecords)
    : rules_(std::move(rules)), cleanupInterval_(cleanupInterval), maxRecords_(maxRecords)
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
        ++rejectedCount_;
        return false;
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
            if (cv_.wait_for(lock, cleanupInterval_) == std::cv_status::no_timeout) {
                if (!running_) break;
            }
        }
        if (!running_) break;

        try {
            auto now = steady_clock::now();
            // 取所有规则中最大的时间窗口作为清理阈值
            auto maxWindowIt = std::max_element(rules_.begin(), rules_.end(),
                                                [](const Rule& a, const Rule& b) { return a.windowSize < b.windowSize; });
            auto threshold = now - (maxWindowIt != rules_.end() ? maxWindowIt->windowSize : seconds(60));

            // 单次持锁遍历，统一完成过期条目清理与容量上限裁剪
            std::unique_lock lock(mutex_);
            for (auto it = records_.begin(); it != records_.end(); ) {
                auto& timestamps = it->second;
                auto eraseIt = std::upper_bound(timestamps.begin(), timestamps.end(), threshold);
                if (eraseIt != timestamps.begin()) {
                    timestamps.erase(timestamps.begin(), eraseIt);
                }
                if (timestamps.empty()) {
                    it = records_.erase(it);
                } else {
                    ++it;
                }
            }

            // 若记录表仍超容，按最久远时间戳逐条驱逐
            while (records_.size() > maxRecords_) {
                auto oldest = records_.begin();
                for (auto it = records_.begin(); it != records_.end(); ++it) {
                    if (it->second.empty()) continue;
                    if (oldest->second.empty() || it->second.front() < oldest->second.front()) {
                        oldest = it;
                    }
                }
                if (oldest != records_.end() && !oldest->second.empty()) {
                    records_.erase(oldest);
                } else {
                    break;
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("RateLimiter cleanup error: %s", e.what());
        } catch (...) {
            LOG_ERROR("RateLimiter cleanup unknown error");
        }
    }
}