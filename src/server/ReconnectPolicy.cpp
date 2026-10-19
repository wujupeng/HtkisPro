#include "ReconnectPolicy.h"
#include <algorithm>

uint32_t ReconnectPolicy::calculateBackoff(uint32_t attempt, const Config::TimeoutConfig& cfg) {
    if (attempt == 0) return cfg.retryInitialBackoffMs;
    uint64_t backoff = static_cast<uint64_t>(cfg.retryInitialBackoffMs) << attempt;
    return static_cast<uint32_t>(std::min(backoff, static_cast<uint64_t>(cfg.retryMaxBackoffMs)));
}

uint32_t ReconnectPolicy::getMaxRetries(const Config::TimeoutConfig& cfg) {
    return cfg.retryCount;
}

bool ReconnectPolicy::shouldRetry(int errorCode) {
    return errorCode != 0;
}