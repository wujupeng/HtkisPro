#pragma once

#include <chrono>
#include <mutex>
#include <cstdint>

class TokenBucket {
public:
    TokenBucket() = default;
    explicit TokenBucket(uint64_t rateBytesPerSec);

    uint64_t tryAcquire(uint64_t bytes);
    void refill();
    void setRate(uint64_t rateBytesPerSec);
    uint64_t getRate() const { return m_rate; }
    uint64_t getTokens() const { return m_tokens; }

private:
    mutable std::mutex m_mutex;
    uint64_t m_rate{0};
    uint64_t m_capacity{0};
    uint64_t m_tokens{0};
    std::chrono::steady_clock::time_point m_lastRefillTime;

    void refillLocked();
};