#include "AuthFailureTracker.h"
#include "Logger.h"

bool AuthFailureTracker::isBlocked(const std::string& clientIp) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bans.find(clientIp);
    if (it == m_bans.end()) return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.banTime).count();
    if (elapsed >= it->second.banDurationSeconds) {
        return false;
    }
    return true;
}

void AuthFailureTracker::recordFailure(const std::string& clientIp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();

    auto& records = m_failures[clientIp];
    records.push_back({now});

    records.erase(std::remove_if(records.begin(), records.end(),
        [now](const FailureRecord& r) {
            return std::chrono::duration_cast<std::chrono::seconds>(now - r.timestamp).count() > WINDOW_SECONDS;
        }), records.end());

    if (static_cast<int>(records.size()) >= FAILURE_THRESHOLD) {
        if (m_bans.find(clientIp) == m_bans.end() ||
            std::chrono::duration_cast<std::chrono::seconds>(now - m_bans[clientIp].banTime).count() >= m_bans[clientIp].banDurationSeconds) {
            m_bans[clientIp] = {now, BAN_SECONDS};
            records.clear();
            Logger::getInstance().warn("[AUTH] IP " + clientIp + " banned for " +
                std::to_string(BAN_SECONDS) + "s due to " + std::to_string(FAILURE_THRESHOLD) +
                " failures in " + std::to_string(WINDOW_SECONDS) + "s");
        }
    }
}

std::vector<std::pair<std::string, int>> AuthFailureTracker::getBlocked() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, int>> result;
    auto now = std::chrono::steady_clock::now();

    for (const auto& [ip, ban] : m_bans) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ban.banTime).count();
        int remaining = ban.banDurationSeconds - static_cast<int>(elapsed);
        if (remaining > 0) {
            result.push_back({ip, remaining});
        }
    }
    return result;
}