#pragma once

#include <string>
#include <functional>
#include <deque>
#include <mutex>
#include <cstdint>
#include <ctime>

struct StabilityMetricsSnapshot {
    double connectFailureRate{0.0};
    double authFailureRate{0.0};
    double abnormalDisconnectRate{0.0};
    uint32_t activeSessionCount{0};
    std::string alertLevel;
};

class StabilityMetrics {
public:
    using AlertCallback = std::function<void(const std::string& alertLevel, const std::string& message)>;

    static StabilityMetrics& getInstance() {
        static StabilityMetrics instance;
        return instance;
    }

    StabilityMetricsSnapshot getSnapshot();
    void setAlertCallback(AlertCallback cb);

    void recordConnectAttempt();
    void recordConnectFailure();
    void recordAuthAttempt();
    void recordAuthFailure();
    void recordDisconnect();
    void recordAbnormalDisconnect();

private:
    StabilityMetrics() = default;

    struct EventWindow {
        std::deque<std::time_t> attempts;
        std::deque<std::time_t> failures;
    };

    mutable std::mutex m_mutex;
    EventWindow m_connect;
    EventWindow m_auth;
    EventWindow m_disconnect;
    AlertCallback m_alert_cb;

    static void prune(EventWindow& w, std::time_t cutoff);
    static double rate(const EventWindow& w);
    static constexpr std::time_t WINDOW_SECONDS = 60;
};