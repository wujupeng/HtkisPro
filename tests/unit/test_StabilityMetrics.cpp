#include <gtest/gtest.h>
#include "StabilityMetrics.h"
#include <thread>
#include <chrono>

TEST(StabilityMetricsTest, InitialState) {
    auto& metrics = StabilityMetrics::getInstance();
    auto snap = metrics.getSnapshot();
    EXPECT_EQ(snap.connectFailureRate, 0.0);
    EXPECT_EQ(snap.authFailureRate, 0.0);
    EXPECT_EQ(snap.alertLevel, "OK");
}

TEST(StabilityMetricsTest, RecordConnectFailure) {
    auto& metrics = StabilityMetrics::getInstance();
    for (int i = 0; i < 10; ++i) metrics.recordConnectAttempt();
    for (int i = 0; i < 5; ++i) metrics.recordConnectFailure();
    auto snap = metrics.getSnapshot();
    EXPECT_NEAR(snap.connectFailureRate, 50.0, 0.1);
}

TEST(StabilityMetricsTest, AlertOnHighFailureRate) {
    auto& metrics = StabilityMetrics::getInstance();
    for (int i = 0; i < 10; ++i) metrics.recordAuthAttempt();
    for (int i = 0; i < 9; ++i) metrics.recordAuthFailure();
    auto snap = metrics.getSnapshot();
    EXPECT_EQ(snap.alertLevel, "WARNING");
}

TEST(StabilityMetricsTest, AlertCallback) {
    auto& metrics = StabilityMetrics::getInstance();
    bool called = false;
    metrics.setAlertCallback([&called](const std::string& level, const std::string&) {
        if (level == "WARNING") called = true;
    });
    for (int i = 0; i < 10; ++i) metrics.recordAuthAttempt();
    for (int i = 0; i < 9; ++i) metrics.recordAuthFailure();
    metrics.getSnapshot();
    EXPECT_TRUE(called);
}