#pragma once

#include "Config.h"
#include <asio.hpp>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <memory>
#include <optional>
#include <list>

class ConnectionPool {
public:
    static ConnectionPool& getInstance() {
        static ConnectionPool instance;
        return instance;
    }

    std::optional<asio::ip::tcp::socket> acquire(
        asio::io_context& io,
        const std::string& host, uint16_t port,
        const std::string& username);

    void release(
        asio::ip::tcp::socket socket,
        const std::string& host, uint16_t port,
        const std::string& username);

    void refreshConfig(std::shared_ptr<const Config::ConnectionPoolConfig> cfg);
    size_t size() const;
    void clear();

private:
    ConnectionPool() = default;

    struct PoolKey {
        std::string host;
        uint16_t port;
        std::string username;

        bool operator==(const PoolKey& other) const {
            return host == other.host && port == other.port && username == other.username;
        }
    };

    struct PoolKeyHash {
        size_t operator()(const PoolKey& k) const {
            return std::hash<std::string>()(k.host) ^
                   (std::hash<uint16_t>()(k.port) << 1) ^
                   (std::hash<std::string>()(k.username) << 2);
        }
    };

    struct PoolEntry {
        asio::ip::tcp::socket socket;
        std::chrono::steady_clock::time_point lastUsed;

        PoolEntry(asio::ip::tcp::socket s, std::chrono::steady_clock::time_point t)
            : socket(std::move(s)), lastUsed(t) {}
    };

    mutable std::mutex m_mutex;
    std::unordered_map<PoolKey, std::deque<PoolEntry>, PoolKeyHash> m_pool;
    Config::ConnectionPoolConfig m_config;
    std::list<PoolKey> m_lru;
    std::unordered_map<PoolKey, std::list<PoolKey>::iterator, PoolKeyHash> m_lruIndex;

    PoolKey makeKey(const std::string& host, uint16_t port, const std::string& username) const;
    void evictExpired();
    void evictLRU();
    bool isSocketHealthy(asio::ip::tcp::socket& socket) const;
};