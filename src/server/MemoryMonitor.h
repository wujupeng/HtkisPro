#pragma once

#include <cstdint>
#include <ctime>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <utility>

struct MemoryReport {
    uint64_t baselineBytes{0};
    uint64_t peakBytes{0};
    uint64_t endBytes{0};
    double growthPercent{0.0};
    bool leakSuspected{false};
    std::vector<std::pair<std::time_t, uint64_t>> samples;
};

class MemoryMonitor {
public:
    static MemoryMonitor& getInstance() {
        static MemoryMonitor instance;
        return instance;
    }

    void start(uint32_t intervalSeconds);
    MemoryReport stop();

private:
    MemoryMonitor() = default;

    void sampleLoop();

    std::atomic<bool> m_running{false};
    std::thread m_thread;
    uint32_t m_interval{5};
    std::mutex m_mutex;
    std::vector<std::pair<std::time_t, uint64_t>> m_samples;

    static uint64_t getCurrentMemoryUsage();
};