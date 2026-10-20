#include <gtest/gtest.h>
#include "MemoryMonitor.h"
#include <thread>
#include <chrono>

TEST(MemoryMonitorTest, StartStopReturnsReport) {
    auto& monitor = MemoryMonitor::getInstance();
    monitor.start(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    auto report = monitor.stop();
    EXPECT_FALSE(report.samples.empty());
    EXPECT_GT(report.baselineBytes, 0);
    EXPECT_GE(report.peakBytes, report.baselineBytes);
}

TEST(MemoryMonitorTest, GrowthPercentCalculation) {
    auto& monitor = MemoryMonitor::getInstance();
    monitor.start(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    auto report = monitor.stop();
    EXPECT_GE(report.growthPercent, 0.0);
}

TEST(MemoryMonitorTest, LeakSuspectedFlag) {
    auto& monitor = MemoryMonitor::getInstance();
    monitor.start(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    auto report = monitor.stop();
    EXPECT_TRUE(report.leakSuspected == (report.growthPercent > 50.0));
}