#include <gtest/gtest.h>
#include "DnsCache.h"
#include "Config.h"
#include <asio.hpp>
#include <memory>
#include <thread>
#include <chrono>

static std::vector<asio::ip::tcp::endpoint> makeEndpoints(const std::string& ip, int port) {
    return {asio::ip::tcp::endpoint(asio::ip::make_address(ip), port)};
}

static void enableDnsCache(uint32_t ttl = 300, uint32_t maxEntries = 4096) {
    Config::DnsCacheConfig cfg;
    cfg.enabled = true;
    cfg.fixedTtlSeconds = ttl;
    cfg.maxEntries = maxEntries;
    DnsCache::getInstance().refreshConfig(
        std::make_shared<const Config::DnsCacheConfig>(cfg));
}

TEST(DnsCacheTest, PutAndLookup) {
    enableDnsCache();
    DnsCache::getInstance().clear();
    auto eps = makeEndpoints("1.2.3.4", 80);
    DnsCache::getInstance().put("example.com", eps);
    auto result = DnsCache::getInstance().lookup("example.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1u);
    EXPECT_EQ((*result)[0].address().to_string(), "1.2.3.4");
}

TEST(DnsCacheTest, MissOnUnknownHost) {
    enableDnsCache();
    DnsCache::getInstance().clear();
    DnsCache::getInstance().put("known.com", makeEndpoints("1.1.1.1", 80));
    auto result = DnsCache::getInstance().lookup("unknown.com");
    EXPECT_FALSE(result.has_value());
}

TEST(DnsCacheTest, TtlExpiry) {
    enableDnsCache(1);
    DnsCache::getInstance().clear();
    DnsCache::getInstance().put("expire.com", makeEndpoints("1.1.1.1", 80), 1);
    auto r1 = DnsCache::getInstance().lookup("expire.com");
    EXPECT_TRUE(r1.has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    auto r2 = DnsCache::getInstance().lookup("expire.com");
    EXPECT_FALSE(r2.has_value());
}

TEST(DnsCacheTest, ClearEmptiesCache) {
    enableDnsCache();
    DnsCache::getInstance().put("a.com", makeEndpoints("1.1.1.1", 80));
    DnsCache::getInstance().put("b.com", makeEndpoints("2.2.2.2", 80));
    EXPECT_GT(DnsCache::getInstance().size(), 0u);
    DnsCache::getInstance().clear();
    EXPECT_EQ(DnsCache::getInstance().size(), 0u);
}

TEST(DnsCacheTest, LruEviction) {
    enableDnsCache(9999, 3);
    DnsCache::getInstance().clear();
    DnsCache::getInstance().put("a.com", makeEndpoints("1.1.1.1", 80));
    DnsCache::getInstance().put("b.com", makeEndpoints("2.2.2.2", 80));
    DnsCache::getInstance().put("c.com", makeEndpoints("3.3.3.3", 80));
    EXPECT_EQ(DnsCache::getInstance().size(), 3u);
    DnsCache::getInstance().lookup("a.com");
    DnsCache::getInstance().put("d.com", makeEndpoints("4.4.4.4", 80));
    EXPECT_EQ(DnsCache::getInstance().size(), 3u);
    EXPECT_FALSE(DnsCache::getInstance().lookup("a.com").has_value());
    EXPECT_TRUE(DnsCache::getInstance().lookup("b.com").has_value());
}

TEST(DnsCacheTest, HitMissCounters) {
    enableDnsCache();
    DnsCache::getInstance().clear();
    DnsCache::getInstance().put("hit.com", makeEndpoints("1.1.1.1", 80));
    DnsCache::getInstance().lookup("hit.com");
    DnsCache::getInstance().lookup("miss.com");
    EXPECT_GT(DnsCache::getInstance().getHits(), 0u);
    EXPECT_GT(DnsCache::getInstance().getMisses(), 0u);
}

TEST(DnsCacheTest, MultipleEndpoints) {
    enableDnsCache();
    DnsCache::getInstance().clear();
    std::vector<asio::ip::tcp::endpoint> eps;
    eps.push_back(asio::ip::tcp::endpoint(asio::ip::make_address("1.1.1.1"), 80));
    eps.push_back(asio::ip::tcp::endpoint(asio::ip::make_address("2.2.2.2"), 80));
    DnsCache::getInstance().put("multi.com", eps);
    auto result = DnsCache::getInstance().lookup("multi.com");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
}