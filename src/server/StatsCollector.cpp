#include "StatsCollector.h"
#include "Logger.h"

void StatsCollector::onSessionStart(uint64_t sessionId, const std::string& username,
                                     const std::string& protocol, const std::string& target,
                                     const std::string& clientAddr,
                                     ProtocolType protocolType,
                                     const std::string& relayNodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    InternalSession s;
    s.sessionId = sessionId;
    s.username = username;
    s.protocol = protocol;
    s.target = target;
    s.clientAddr = clientAddr;
    s.startTime = std::time(nullptr);
    s.lastRateTime = s.startTime;
    s.protocolType = protocolType;
    s.relayNodeId = relayNodeId;
    m_sessions[sessionId] = std::move(s);
    m_activeCount.fetch_add(1);
    m_totalConnects.fetch_add(1);
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
    m_totalDisconnects.fetch_add(1);
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

        auto& ps = snap.protocolStats[s.protocolType];
        ps.activeSessions++;
        ps.bytesUp += s.bytesUp;
        ps.bytesDown += s.bytesDown;

        if (!s.relayNodeId.empty()) {
            auto& rs = snap.relayStats[s.relayNodeId];
            rs.activeSessions++;
            rs.bytesUp += s.bytesUp;
            rs.bytesDown += s.bytesDown;
        }

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

StabilitySnapshot StatsCollector::getStabilitySnapshot() const {
    StabilitySnapshot snap;
    uint64_t totalConns = m_totalConnects.load();
    uint64_t totalAuths = m_totalAuths.load();
    uint64_t totalDisconns = m_totalDisconnects.load();
    if (totalConns > 0) snap.connectFailureRate = static_cast<double>(m_connectFailures.load()) / totalConns;
    if (totalAuths > 0) snap.authFailureRate = static_cast<double>(m_authFailures.load()) / totalAuths;
    if (totalDisconns > 0) snap.abnormalDisconnectRate = static_cast<double>(m_abnormalDisconnects.load()) / totalDisconns;
    return snap;
}

void StatsCollector::recordConnectFailure() {
    m_connectFailures.fetch_add(1);
    m_totalConnects.fetch_add(1);
}

void StatsCollector::recordAuthFailure() {
    m_authFailures.fetch_add(1);
    m_totalAuths.fetch_add(1);
}

void StatsCollector::recordAbnormalDisconnect() {
    m_abnormalDisconnects.fetch_add(1);
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