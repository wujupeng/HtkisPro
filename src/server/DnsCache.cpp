#include "DnsCache.h"
#include "Logger.h"
#include <atomic>

void DnsCache::touchLRU(const std::string& key) {
    auto it = m_lruIndex.find(key);
    if (it != m_lruIndex.end()) {
        m_lru.erase(it->second);
    }
    m_lru.push_front(key);
    m_lruIndex[key] = m_lru.begin();
}

void DnsCache::evictLRU() {
    while (m_cache.size() > m_config.maxEntries && !m_lru.empty()) {
        auto& oldest = m_lru.back();
        m_cache.erase(oldest);
        m_lruIndex.erase(oldest);
        m_lru.pop_back();
    }
}

std::optional<std::vector<asio::ip::tcp::endpoint>> DnsCache::lookup(const std::string& host) {
    if (!m_config.enabled) {
        m_misses.fetch_add(1);
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_cache.find(host);
    if (it == m_cache.end()) {
        m_misses.fetch_add(1);
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() >= it->second.expireTime) {
        m_misses.fetch_add(1);
        return std::nullopt;
    }

    m_hits.fetch_add(1);
    return it->second.endpoints;
}

void DnsCache::put(const std::string& host, const std::vector<asio::ip::tcp::endpoint>& endpoints, uint32_t ttl) {
    if (!m_config.enabled) return;

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    uint32_t effectiveTtl = (ttl > 0) ? ttl : m_config.fixedTtlSeconds;
    CacheEntry entry;
    entry.endpoints = endpoints;
    entry.expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(effectiveTtl);

    m_cache[host] = std::move(entry);
    touchLRU(host);
    evictLRU();
}

void DnsCache::clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_cache.clear();
    m_lru.clear();
    m_lruIndex.clear();
    Logger::getInstance().info("[DNS] Cache cleared");
}

void DnsCache::refreshConfig(std::shared_ptr<const Config::DnsCacheConfig> cfg) {
    if (!cfg) return;
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_config = *cfg;
}

size_t DnsCache::size() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_cache.size();
}