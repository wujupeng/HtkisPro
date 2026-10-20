#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <ctime>
#include "Config.h"
#include "MemoryMonitor.h"
#include "BenchmarkWorker.h"

struct BenchmarkReport {
    double throughputMbps{0.0};
    uint32_t maxConcurrency{0};
    double avgHandshakeLatencyMs{0.0};
    double p99HandshakeLatencyMs{0.0};
    uint64_t totalBytesUp{0};
    uint64_t totalBytesDown{0};
    MemoryReport memoryReport;
    std::time_t startTime{0};
    uint32_t duration{0};
};

class BenchmarkWorker;

class BenchmarkEngine {
public:
    using ProgressCallback = std::function<void(double percent, const std::string& status)>;
    using CompleteCallback = std::function<void(const BenchmarkReport&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    static BenchmarkEngine& getInstance() {
        static BenchmarkEngine instance;
        return instance;
    }

    void run(const Config::BenchmarkConfig& cfg,
             ProgressCallback onProgress,
             CompleteCallback onComplete,
             ErrorCallback onError);
    void cancel();

private:
    BenchmarkEngine() = default;

    std::atomic<bool> m_running{false};
    std::vector<std::unique_ptr<BenchmarkWorker>> m_workers;
};