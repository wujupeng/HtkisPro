#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <vector>
#include <utility>
#include <atomic>
#include "Config.h"

struct UserInfo {
    std::string password;
    std::string passwordHash;
    std::string salt;
    std::time_t expire_time;
    bool active;
    Config::UserRateLimit rateLimit;
};

class UserManager {
public:
    static UserManager& getInstance();

    void load(const std::string& filepath);
    void save();

    bool addUser(const std::string& username, const std::string& password, int days);
    bool removeUser(const std::string& username);
    bool authenticate(const std::string& username, const std::string& password);
    void listUsers();
    bool setStatus(const std::string& username, bool active);
    bool updatePassword(const std::string& username, const std::string& newPassword);
    bool updateExpire(const std::string& username, int days);
    bool updateRateLimit(const std::string& username, const Config::UserRateLimit& limit);

    std::vector<std::pair<std::string, UserInfo>> getAllUsers();
    uint64_t getVersion() const { return m_version.load(); }

private:
    UserManager() = default;
    ~UserManager() = default;

    std::string m_filepath;
    std::unordered_map<std::string, UserInfo> m_users;
    mutable std::mutex m_mutex;
    std::atomic<uint64_t> m_version{0};

    void incrementVersion() { m_version.fetch_add(1); }
    bool needsMigration() const;
};
