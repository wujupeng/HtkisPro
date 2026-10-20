#include "BenchmarkEngine.h"
#include "BenchmarkWorker.h"
#include "Logger.h"
#include <algorithm>
#include <numeric>

void BenchmarkEngine::run(const Config::BenchmarkConfig& cfg,
                          ProgressCallback onProgress,
                          CompleteCallback onComplete,
                          ErrorCallback onError) {
    if (m_running) {
        if (onError) onError("Benchmark already running");
        return;
    }
    m_running = true;

    std::thread([this, cfg, onProgress, onComplete, onError]() {
        BenchmarkReport report;
        report.startTime = std::time(nullptr);
        report.duration = cfg.durationSeconds;

        MemoryMonitor::getInstance().start(cfg.memoryMonitorInterval);

        asio::io_context io;
        auto guard = asio::make_work_guard(io);
        std::vector<std::thread> threads;
        unsigned nThreads = std::thread::hardware_concurrency();
        if (nThreads < 2) nThreads = 2;
        for (unsigned i = 0; i < nThreads; ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }

        m_workers.clear();
        m_workers.reserve(cfg.concurrency);
        std::vector<double> latencies;

        for (uint32_t i = 0; i < cfg.concurrency; ++i) {
            auto worker = std::make_unique<BenchmarkWorker>(io, cfg.protocol, cfg.targetHost, cfg.targetPort);
            worker->run();
            m_workers.push_back(std::move(worker));
        }

        for (uint32_t elapsed = 0; elapsed < cfg.durationSeconds && m_running; ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (onProgress) {
                onProgress(static_cast<double>(elapsed + 1) / cfg.durationSeconds * 100.0,
                           "Running: " + std::to_string(elapsed + 1) + "/" + std::to_string(cfg.durationSeconds) + "s");
            }
        }

        for (auto& w : m_workers) w->stop();
        guard.reset();
        io.stop();
        for (auto& t : threads) if (t.joinable()) t.join();

        uint64_t totalUp = 0, totalDown = 0;
        for (const auto& w : m_workers) {
            totalUp += w->getBytesUp();
            totalDown += w->getBytesDown();
            latencies.push_back(w->getHandshakeLatencyMs());
        }

        report.totalBytesUp = totalUp;
        report.totalBytesDown = totalDown;
        report.maxConcurrency = cfg.concurrency;

        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            report.avgHandshakeLatencyMs = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
            size_t p99Idx = static_cast<size_t>(latencies.size() * 0.99);
            if (p99Idx >= latencies.size()) p99Idx = latencies.size() - 1;
            report.p99HandshakeLatencyMs = latencies[p99Idx];
        }

        double totalMB = static_cast<double>(totalUp + totalDown) / (1024.0 * 1024.0);
        report.throughputMbps = totalMB / cfg.durationSeconds;

        report.memoryReport = MemoryMonitor::getInstance().stop();

        m_workers.clear();
        m_running = false;

        Logger::getInstance().info("[BENCH] Done: throughput=" + std::to_string(report.throughputMbps) +
            " MB/s avgLatency=" + std::to_string(report.avgHandshakeLatencyMs) + "ms");

        if (onComplete) onComplete(report);
    }).detach();
}

void BenchmarkEngine::cancel() {
    m_running = false;
    for (auto& w : m_workers) w->stop();
}