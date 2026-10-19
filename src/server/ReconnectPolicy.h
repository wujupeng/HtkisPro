#pragma once

#include "Config.h"
#include <cstdint>
#include <chrono>

class ReconnectPolicy {
public:
    static uint32_t calculateBackoff(uint32_t attempt, const Config::TimeoutConfig& cfg);
    static uint32_t getMaxRetries(const Config::TimeoutConfig& cfg);
    static bool shouldRetry(int errorCode);
};