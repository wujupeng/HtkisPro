#include "TokenBucket.h"

TokenBucket::TokenBucket(uint64_t rateBytesPerSec)
    : m_rate(rateBytesPerSec),
      m_capacity(rateBytesPerSec == 0 ? 0 : rateBytesPerSec * 11 / 10),
      m_tokens(m_capacity),
      m_lastRefillTime(std::chrono::steady_clock::now()) {
}

void TokenBucket::refill() {
    std::lock_guard<std::mutex> lock(m_mutex);
    refillLocked();
}

void TokenBucket::refillLocked() {
    if (m_rate == 0) return;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_lastRefillTime).count();
    if (elapsed <= 0) return;
    uint64_t added = static_cast<uint64_t>(elapsed) * m_rate / 1000000000ULL;
    m_tokens = (m_tokens + added > m_capacity) ? m_capacity : m_tokens + added;
    m_lastRefillTime = now;
}

uint64_t TokenBucket::tryAcquire(uint64_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_rate == 0) return bytes;
    refillLocked();
    uint64_t actual = (bytes < m_tokens) ? bytes : m_tokens;
    m_tokens -= actual;
    return actual;
}

void TokenBucket::setRate(uint64_t rateBytesPerSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rate = rateBytesPerSec;
    m_capacity = (rateBytesPerSec == 0) ? 0 : rateBytesPerSec * 11 / 10;
    m_tokens = m_capacity;
    m_lastRefillTime = std::chrono::steady_clock::now();
}