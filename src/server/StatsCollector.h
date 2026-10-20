#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <cstdint>
#include <ctime>
#include <map>
#include "ProtocolDetector.h"

struct UserStats {
    std::string username;
    uint64_t bytesUp = 0;
    uint64_t bytesDown = 0;
    uint64_t rateUp = 0;
    uint64_t rateDown = 0;
};

struct SessionInfo {
    uint64_t sessionId;
    std::string username;
    std::string protocol;
    std::string target;
    std::string clientAddr;
    uint64_t bytesUp = 0;
    uint64_t bytesDown = 0;
    std::time_t startTime = 0;
    bool active = true;
    ProtocolType protocolType = ProtocolType::UNKNOWN;
    std::string relayNodeId;
};

struct ProtocolStat {
    uint32_t activeSessions = 0;
    uint64_t bytesUp = 0;
    uint64_t bytesDown = 0;
};

struct RelayStat {
    uint32_t activeSessions = 0;
    uint64_t bytesUp = 0;
    uint64_t bytesDown = 0;
    uint64_t latency = 0;
};

struct StatsSnapshot {
    uint64_t globalBytesUp = 0;
    uint64_t globalBytesDown = 0;
    uint64_t globalRateUp = 0;
    uint64_t globalRateDown = 0;
    uint32_t activeSessionCount = 0;
    std::vector<UserStats> userStats;
    std::vector<SessionInfo> activeSessions;
    std::map<ProtocolType, ProtocolStat> protocolStats;
    std::map<std::string, RelayStat> relayStats;
};

struct StabilitySnapshot {
    double connectFailureRate = 0.0;
    double authFailureRate = 0.0;
    double abnormalDisconnectRate = 0.0;
};

class StatsCollector {
public:
    static StatsCollector& getInstance() {
        static StatsCollector instance;
        return instance;
    }

    void onSessionStart(uint64_t sessionId, const std::string& username,
                        const std::string& protocol, const std::string& target,
                        const std::string& clientAddr,
                        ProtocolType protocolType = ProtocolType::UNKNOWN,
                        const std::string& relayNodeId = "");
    void onSessionUpdate(uint64_t sessionId, uint64_t deltaUp, uint64_t deltaDown);
    void onSessionClose(uint64_t sessionId, const std::string& reason);

    StatsSnapshot getSnapshot() const;
    StabilitySnapshot getStabilitySnapshot() const;
    uint32_t getActiveSessionCount() const { return m_activeCount.load(); }

    void recordConnectFailure();
    void recordAuthFailure();
    void recordAbnormalDisconnect();
    void compact();

private:
    StatsCollector() = default;

    struct InternalSession : SessionInfo {
        uint64_t lastBytesUp = 0;
        uint64_t lastBytesDown = 0;
        std::time_t lastRateTime = 0;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, InternalSession> m_sessions;
    std::atomic<uint32_t> m_activeCount{0};
    std::atomic<uint64_t> m_globalBytesUp{0};
    std::atomic<uint64_t> m_globalBytesDown{0};
    std::atomic<uint64_t> m_nextSessionId{1};

    std::atomic<uint64_t> m_connectFailures{0};
    std::atomic<uint64_t> m_authFailures{0};
    std::atomic<uint64_t> m_abnormalDisconnects{0};
    std::atomic<uint64_t> m_totalConnects{0};
    std::atomic<uint64_t> m_totalAuths{0};
    std::atomic<uint64_t> m_totalDisconnects{0};
};