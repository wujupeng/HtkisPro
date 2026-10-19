#include "QosScheduler.h"
#include <algorithm>

void QosScheduler::registerSession(uint64_t sessionId, const std::string& username, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[sessionId] = {username, priority};
}

void QosScheduler::unregisterSession(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.erase(sessionId);
}

void QosScheduler::updatePriority(uint64_t sessionId, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        it->second.priority = priority;
    }
}

std::vector<uint64_t> QosScheduler::getOrderedSessions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<int, uint64_t>> sorted;
    sorted.reserve(m_sessions.size());
    for (const auto& [id, info] : m_sessions) {
        sorted.push_back({info.priority, id});
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<uint64_t> result;
    result.reserve(sorted.size());
    for (const auto& [priority, id] : sorted) {
        result.push_back(id);
    }
    return result;
}

int QosScheduler::getPriority(uint64_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.priority : 0;
}