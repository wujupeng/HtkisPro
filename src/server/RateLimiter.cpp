#include "RateLimiter.h"
#include "Logger.h"

AcquireResult RateLimiter::acquire(const std::string& username, uint64_t bytes, bool isUp) {
    std::lock_guard<std::mutex> lock(m_mutex);

    TokenBucket& globalBucket = isUp ? m_globalUpBucket : m_globalDownBucket;
    uint64_t afterGlobal = globalBucket.tryAcquire(bytes);
    uint64_t immediate = afterGlobal;
    uint64_t delayed = bytes - afterGlobal;

    if (!username.empty()) {
        auto it = m_userBuckets.find(username);
        if (it != m_userBuckets.end()) {
            TokenBucket& userBucket = isUp ? it->second.first : it->second.second;
            uint64_t afterUser = userBucket.tryAcquire(immediate);
            immediate = afterUser;
            delayed += (immediate - afterUser);
        }
    }

    return {immediate, delayed};
}

void RateLimiter::refreshConfig(std::shared_ptr<const Config::RateLimitConfig> cfg) {
    if (!cfg) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = *cfg;

    m_globalUpBucket.setRate(cfg->globalUpLimit);
    m_globalDownBucket.setRate(cfg->globalDownLimit);

    for (const auto& [name, limit] : cfg->userLimits) {
        auto& buckets = m_userBuckets[name];
        buckets.first.setRate(limit.upLimit);
        buckets.second.setRate(limit.downLimit);
    }

    Logger::getInstance().info("[RATE] Refreshed: globalUp=" +
        std::to_string(cfg->globalUpLimit) + " globalDown=" +
        std::to_string(cfg->globalDownLimit) +
        " users=" + std::to_string(cfg->userLimits.size()));
}