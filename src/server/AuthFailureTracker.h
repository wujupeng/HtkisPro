#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <utility>

class AuthFailureTracker {
public:
    static AuthFailureTracker& getInstance() {
        static AuthFailureTracker instance;
        return instance;
    }

    bool isBlocked(const std::string& clientIp) const;
    void recordFailure(const std::string& clientIp);
    std::vector<std::pair<std::string, int>> getBlocked() const;

    static constexpr int WINDOW_SECONDS = 60;
    static constexpr int FAILURE_THRESHOLD = 10;
    static constexpr int BAN_SECONDS = 300;

private:
    AuthFailureTracker() = default;

    struct FailureRecord {
        std::chrono::steady_clock::time_point timestamp;
    };

    struct BanRecord {
        std::chrono::steady_clock::time_point banTime;
        int banDurationSeconds;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<FailureRecord>> m_failures;
    std::unordered_map<std::string, BanRecord> m_bans;
};