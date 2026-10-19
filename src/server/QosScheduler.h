#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <cstdint>

class QosScheduler {
public:
    static QosScheduler& getInstance() {
        static QosScheduler instance;
        return instance;
    }

    void registerSession(uint64_t sessionId, const std::string& username, int priority);
    void unregisterSession(uint64_t sessionId);
    void updatePriority(uint64_t sessionId, int priority);

    std::vector<uint64_t> getOrderedSessions() const;
    int getPriority(uint64_t sessionId) const;

private:
    QosScheduler() = default;

    struct SessionInfo {
        std::string username;
        int priority;
    };

    mutable std::mutex m_mutex;
    std::map<uint64_t, SessionInfo> m_sessions;
};