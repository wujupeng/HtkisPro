#pragma once

#include "Config.h"
#include <asio.hpp>
#include <string>
#include <vector>
#include <optional>
#include <list>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <memory>
#include <atomic>

class DnsCache {
public:
    static DnsCache& getInstance() {
        static DnsCache instance;
        return instance;
    }

    std::optional<std::vector<asio::ip::tcp::endpoint>> lookup(const std::string& host);
    void put(const std::string& host, const std::vector<asio::ip::tcp::endpoint>& endpoints, uint32_t ttl = 0);
    void clear();
    void refreshConfig(std::shared_ptr<const Config::DnsCacheConfig> cfg);

    size_t size() const;
    uint64_t getHits() const { return m_hits.load(); }
    uint64_t getMisses() const { return m_misses.load(); }

private:
    DnsCache() = default;

    struct CacheEntry {
        std::vector<asio::ip::tcp::endpoint> endpoints;
        std::chrono::steady_clock::time_point expireTime;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, CacheEntry> m_cache;
    std::list<std::string> m_lru;
    std::unordered_map<std::string, std::list<std::string>::iterator> m_lruIndex;
    Config::DnsCacheConfig m_config;
    std::atomic<uint64_t> m_hits{0};
    std::atomic<uint64_t> m_misses{0};

    void evictLRU();
    void touchLRU(const std::string& key);
};