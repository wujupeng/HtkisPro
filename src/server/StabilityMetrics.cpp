#include "StabilityMetrics.h"
#include "StatsCollector.h"
#include "Logger.h"

void StabilityMetrics::prune(EventWindow& w, std::time_t cutoff) {
    while (!w.attempts.empty() && w.attempts.front() < cutoff) w.attempts.pop_front();
    while (!w.failures.empty() && w.failures.front() < cutoff) w.failures.pop_front();
}

double StabilityMetrics::rate(const EventWindow& w) {
    if (w.attempts.empty()) return 0.0;
    return static_cast<double>(w.failures.size()) / w.attempts.size() * 100.0;
}

StabilityMetricsSnapshot StabilityMetrics::getSnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::time_t now = std::time(nullptr);
    std::time_t cutoff = now - WINDOW_SECONDS;

    prune(m_connect, cutoff);
    prune(m_auth, cutoff);
    prune(m_disconnect, cutoff);

    StabilityMetricsSnapshot snap;
    snap.connectFailureRate = rate(m_connect);
    snap.authFailureRate = rate(m_auth);
    snap.abnormalDisconnectRate = rate(m_disconnect);
    snap.activeSessionCount = StatsCollector::getInstance().getActiveSessionCount();

    if (snap.connectFailureRate > 10.0 || snap.authFailureRate > 10.0 || snap.abnormalDisconnectRate > 10.0) {
        snap.alertLevel = "WARNING";
        if (m_alert_cb) {
            std::string msg = "connectFail=" + std::to_string(snap.connectFailureRate) +
                              "% authFail=" + std::to_string(snap.authFailureRate) +
                              "% abnormalDisconnect=" + std::to_string(snap.abnormalDisconnectRate) + "%";
            m_alert_cb(snap.alertLevel, msg);
        }
    } else {
        snap.alertLevel = "OK";
    }

    return snap;
}

void StabilityMetrics::setAlertCallback(AlertCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alert_cb = std::move(cb);
}

void StabilityMetrics::recordConnectAttempt() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connect.attempts.push_back(std::time(nullptr));
}

void StabilityMetrics::recordConnectFailure() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connect.failures.push_back(std::time(nullptr));
}

void StabilityMetrics::recordAuthAttempt() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auth.attempts.push_back(std::time(nullptr));
}

void StabilityMetrics::recordAuthFailure() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auth.failures.push_back(std::time(nullptr));
}

void StabilityMetrics::recordDisconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_disconnect.attempts.push_back(std::time(nullptr));
}

void StabilityMetrics::recordAbnormalDisconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_disconnect.failures.push_back(std::time(nullptr));
}