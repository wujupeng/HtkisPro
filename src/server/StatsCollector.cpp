#include "StatsCollector.h"
#include "Logger.h"

void StatsCollector::onSessionStart(uint64_t sessionId, const std::string& username,
                                     const std::string& protocol, const std::string& target,
                                     const std::string& clientAddr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    InternalSession s;
    s.sessionId = sessionId;
    s.username = username;
    s.protocol = protocol;
    s.target = target;
    s.clientAddr = clientAddr;
    s.startTime = std::time(nullptr);
    s.lastRateTime = s.startTime;
    m_sessions[sessionId] = std::move(s);
    m_activeCount.fetch_add(1);
}

void StatsCollector::onSessionUpdate(uint64_t sessionId, uint64_t deltaUp, uint64_t deltaDown) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;
    it->second.bytesUp += deltaUp;
    it->second.bytesDown += deltaDown;
    m_globalBytesUp.fetch_add(deltaUp);
    m_globalBytesDown.fetch_add(deltaDown);
}

void StatsCollector::onSessionClose(uint64_t sessionId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;
    it->second.active = false;
    m_activeCount.fetch_sub(1);
}

StatsSnapshot StatsCollector::getSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    StatsSnapshot snap;
    snap.globalBytesUp = m_globalBytesUp.load();
    snap.globalBytesDown = m_globalBytesDown.load();
    snap.activeSessionCount = m_activeCount.load();

    auto now = std::time(nullptr);
    std::unordered_map<std::string, UserStats> userMap;

    for (const auto& [id, s] : m_sessions) {
        if (!s.active) continue;

        snap.activeSessions.push_back(static_cast<SessionInfo>(s));

        auto& us = userMap[s.username];
        us.username = s.username;
        us.bytesUp += s.bytesUp;
        us.bytesDown += s.bytesDown;

        if (now > s.lastRateTime) {
            uint64_t rateUp = (s.bytesUp - s.lastBytesUp) / static_cast<uint64_t>(now - s.lastRateTime);
            uint64_t rateDown = (s.bytesDown - s.lastBytesDown) / static_cast<uint64_t>(now - s.lastRateTime);
            us.rateUp += rateUp;
            us.rateDown += rateDown;
            snap.globalRateUp += rateUp;
            snap.globalRateDown += rateDown;
        }
    }

    for (auto& [name, us] : userMap) {
        snap.userStats.push_back(std::move(us));
    }

    return snap;
}

void StatsCollector::compact() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (!it->second.active) {
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}