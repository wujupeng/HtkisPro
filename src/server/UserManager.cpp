#include "UserManager.h"
#include "HashUtil.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

UserManager& UserManager::getInstance() {
    static UserManager instance;
    return instance;
}

bool UserManager::needsMigration() const {
    for (const auto& [name, info] : m_users) {
        if (info.salt.empty() && !info.password.empty()) return true;
    }
    return false;
}

void UserManager::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filepath = filepath;
    m_users.clear();

    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    bool loaded = false;
    bool migrated = false;

    try {
        json j = json::parse(content);
        for (auto it = j.begin(); it != j.end(); ++it) {
            UserInfo info;
            info.passwordHash = it->value("passwordHash", "");
            info.salt = it->value("salt", "");
            info.password = it->value("password", "");
            info.expire_time = it->value("expireTime", static_cast<std::time_t>(0));
            info.active = it->value("active", true);
            if (it->contains("rateLimit")) {
                info.rateLimit.upLimit = it->value("upLimit", 0ULL);
                info.rateLimit.downLimit = it->value("downLimit", 0ULL);
                info.rateLimit.priority = it->value("priority", 0);
            }
            m_users[it.key()] = info;
        }
        loaded = true;
        Logger::getInstance().info("[USER] Loaded " + std::to_string(m_users.size()) + " users from JSON");
    } catch (const std::exception&) {
    }

    if (!loaded) {
        std::istringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            std::stringstream lineSs(line);
            std::string user, pass;
            time_t expire;
            bool active;
            if (lineSs >> user >> pass >> expire >> active) {
                UserInfo info;
                info.password = pass;
                info.expire_time = expire;
                info.active = active;
                info.salt = HashUtil::generateSalt();
                info.passwordHash = HashUtil::sha256Hex(info.salt, pass);
                info.password.clear();
                m_users[user] = info;
                migrated = true;
            }
        }
        if (!m_users.empty()) {
            Logger::getInstance().info("[USER] Loaded " + std::to_string(m_users.size()) + " users from legacy format, migrated to hashed");
        }
    }

    if (migrated) {
        incrementVersion();
    }
    incrementVersion();
}

void UserManager::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filepath.empty()) return;

    json j = json::object();
    for (const auto& [name, info] : m_users) {
        json user;
        user["passwordHash"] = info.passwordHash;
        user["salt"] = info.salt;
        user["expireTime"] = info.expire_time;
        user["active"] = info.active;
        user["rateLimit"] = {
            {"upLimit", info.rateLimit.upLimit},
            {"downLimit", info.rateLimit.downLimit},
            {"priority", info.rateLimit.priority}
        };
        j[name] = user;
    }

    std::ofstream file(m_filepath);
    if (!file.is_open()) {
        Logger::getInstance().error("[USER] Failed to save users to " + m_filepath);
        return;
    }
    file << j.dump(2);
    Logger::getInstance().info("[USER] Saved " + std::to_string(m_users.size()) + " users to " + m_filepath);
}

bool UserManager::addUser(const std::string& username, const std::string& password, int days) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_users.find(username) != m_users.end()) return false;

    time_t now = time(nullptr);
    time_t expire = now + static_cast<time_t>(days) * 24 * 3600;

    UserInfo info;
    info.salt = HashUtil::generateSalt();
    info.passwordHash = HashUtil::sha256Hex(info.salt, password);
    info.expire_time = expire;
    info.active = true;
    m_users[username] = info;
    incrementVersion();
    return true;
}

bool UserManager::removeUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool removed = m_users.erase(username) > 0;
    if (removed) incrementVersion();
    return removed;
}

bool UserManager::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;

    if (!it->second.active) return false;

    time_t now = time(nullptr);
    if (now > it->second.expire_time) return false;

    if (!it->second.salt.empty() && !it->second.passwordHash.empty()) {
        return HashUtil::sha256Hex(it->second.salt, password) == it->second.passwordHash;
    }

    if (!it->second.password.empty()) {
        if (it->second.password == password) {
            it->second.salt = HashUtil::generateSalt();
            it->second.passwordHash = HashUtil::sha256Hex(it->second.salt, password);
            it->second.password.clear();
            incrementVersion();
            return true;
        }
        return false;
    }

    return false;
}

void UserManager::listUsers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << std::left << std::setw(15) << "Username"
              << std::setw(10) << "Active"
              << std::setw(25) << "Expire Time" << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& [name, info] : m_users) {
        std::string timeStr = std::ctime(&info.expire_time);
        if (!timeStr.empty()) timeStr.pop_back();

        std::cout << std::left << std::setw(15) << name
                  << std::setw(10) << (info.active ? "Yes" : "No")
                  << timeStr << "\n";
    }
}

bool UserManager::setStatus(const std::string& username, bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.active = active;
    incrementVersion();
    return true;
}

bool UserManager::updatePassword(const std::string& username, const std::string& newPassword) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.salt = HashUtil::generateSalt();
    it->second.passwordHash = HashUtil::sha256Hex(it->second.salt, newPassword);
    it->second.password.clear();
    incrementVersion();
    return true;
}

bool UserManager::updateExpire(const std::string& username, int days) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.expire_time = time(nullptr) + static_cast<time_t>(days) * 24 * 3600;
    incrementVersion();
    return true;
}

bool UserManager::updateRateLimit(const std::string& username, const Config::UserRateLimit& limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_users.find(username);
    if (it == m_users.end()) return false;
    it->second.rateLimit = limit;
    incrementVersion();
    return true;
}

std::vector<std::pair<std::string, UserInfo>> UserManager::getAllUsers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, UserInfo>> users;
    users.reserve(m_users.size());
    for (const auto& kv : m_users) {
        users.push_back(kv);
    }
    return users;
}
