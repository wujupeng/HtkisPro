#include "MemoryMonitor.h"
#include "Logger.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

uint64_t MemoryMonitor::getCurrentMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    return 0;
#endif
}

void MemoryMonitor::start(uint32_t intervalSeconds) {
    if (m_running) return;
    m_interval = intervalSeconds;
    m_running = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_samples.clear();
    }
    m_thread = std::thread([this]() { sampleLoop(); });
    Logger::getInstance().info("[MEM] Monitor started, interval=" + std::to_string(m_interval) + "s");
}

void MemoryMonitor::sampleLoop() {
    while (m_running) {
        uint64_t mem = getCurrentMemoryUsage();
        std::time_t now = std::time(nullptr);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_samples.emplace_back(now, mem);
        }
        for (uint32_t i = 0; i < m_interval * 10 && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

MemoryReport MemoryMonitor::stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();

    std::lock_guard<std::mutex> lock(m_mutex);

    MemoryReport report;
    report.samples = m_samples;
    if (m_samples.empty()) return report;

    report.baselineBytes = m_samples.front().second;
    report.endBytes = m_samples.back().second;
    report.peakBytes = 0;
    for (const auto& [t, mem] : m_samples) {
        if (mem > report.peakBytes) report.peakBytes = mem;
    }

    if (report.baselineBytes > 0) {
        report.growthPercent = static_cast<double>(report.endBytes - report.baselineBytes) / report.baselineBytes * 100.0;
    }
    report.leakSuspected = report.growthPercent > 50.0;

    Logger::getInstance().info("[MEM] Report: baseline=" + std::to_string(report.baselineBytes) +
        " peak=" + std::to_string(report.peakBytes) +
        " end=" + std::to_string(report.endBytes) +
        " growth=" + std::to_string(report.growthPercent) + "%" +
        (report.leakSuspected ? " [LEAK SUSPECTED]" : ""));

    return report;
}