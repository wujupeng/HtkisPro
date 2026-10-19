#pragma once

#include "Config.h"
#include "TokenBucket.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

struct AcquireResult {
    uint64_t immediateBytes;
    uint64_t delayedBytes;
};

class RateLimiter {
public:
    static RateLimiter& getInstance() {
        static RateLimiter instance;
        return instance;
    }

    AcquireResult acquire(const std::string& username, uint64_t bytes, bool isUp);
    void refreshConfig(std::shared_ptr<const Config::RateLimitConfig> cfg);

private:
    RateLimiter() = default;

    mutable std::mutex m_mutex;
    TokenBucket m_globalUpBucket;
    TokenBucket m_globalDownBucket;
    std::unordered_map<std::string, std::pair<TokenBucket, TokenBucket>> m_userBuckets;
    Config::RateLimitConfig m_config;
};