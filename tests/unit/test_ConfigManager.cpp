#include <gtest/gtest.h>
#include "ConfigManager.h"
#include "Config.h"
#include <fstream>
#include <cstdio>

namespace Config {

TEST(ConfigTest, DefaultValues) {
    auto cfg = defaultConfig();
    EXPECT_EQ(cfg.ipAcl.mode, AclMode::Disabled);
    EXPECT_EQ(cfg.timeout.firstPacketSeconds, 10u);
    EXPECT_EQ(cfg.timeout.idleSeconds, 300u);
    EXPECT_FALSE(cfg.connectionPool.enabled);
    EXPECT_FALSE(cfg.dnsCache.enabled);
    EXPECT_FALSE(cfg.tls.enabled);
}

TEST(ConfigTest, ValidateDefault) {
    auto cfg = defaultConfig();
    EXPECT_TRUE(validate(cfg));
}

TEST(ConfigTest, AclModeToString) {
    EXPECT_EQ(aclModeToString(AclMode::Disabled), "disabled");
    EXPECT_EQ(aclModeToString(AclMode::Whitelist), "whitelist");
    EXPECT_EQ(aclModeToString(AclMode::Blacklist), "blacklist");
}

TEST(ConfigTest, StringToAclMode) {
    EXPECT_EQ(stringToAclMode("disabled"), AclMode::Disabled);
    EXPECT_EQ(stringToAclMode("whitelist"), AclMode::Whitelist);
    EXPECT_EQ(stringToAclMode("blacklist"), AclMode::Blacklist);
    EXPECT_EQ(stringToAclMode("unknown"), AclMode::Disabled);
}

TEST(ConfigTest, TlsVersionToString) {
    EXPECT_EQ(tlsVersionToString(TlsVersion::TLS_1_0), "1.0");
    EXPECT_EQ(tlsVersionToString(TlsVersion::TLS_1_2), "1.2");
    EXPECT_EQ(tlsVersionToString(TlsVersion::TLS_1_3), "1.3");
}

TEST(ConfigTest, StringToTlsVersion) {
    EXPECT_EQ(stringToTlsVersion("1.0"), TlsVersion::TLS_1_0);
    EXPECT_EQ(stringToTlsVersion("1.2"), TlsVersion::TLS_1_2);
    EXPECT_EQ(stringToTlsVersion("1.3"), TlsVersion::TLS_1_3);
}

TEST(ConfigManagerTest, GetSnapshotReturnsDefault) {
    auto& cm = ConfigManager::getInstance();
    auto snap = cm.getSnapshot();
    ASSERT_NE(snap, nullptr);
    EXPECT_EQ(snap->ipAcl.mode, AclMode::Disabled);
}

TEST(ConfigManagerTest, UpdateIpAcl) {
    auto& cm = ConfigManager::getInstance();
    cm.updateIpAcl([](IpAclConfig& c) {
        c.mode = AclMode::Whitelist;
        c.rules.push_back({"192.168.1.0/24", false});
    });
    auto snap = cm.getSnapshot();
    EXPECT_EQ(snap->ipAcl.mode, AclMode::Whitelist);
    ASSERT_EQ(snap->ipAcl.rules.size(), 1u);
    EXPECT_EQ(snap->ipAcl.rules[0].network, "192.168.1.0/24");
}

TEST(ConfigManagerTest, UpdateTimeout) {
    auto& cm = ConfigManager::getInstance();
    cm.updateTimeout([](TimeoutConfig& c) {
        c.firstPacketSeconds = 30;
        c.idleSeconds = 600;
    });
    auto snap = cm.getSnapshot();
    EXPECT_EQ(snap->timeout.firstPacketSeconds, 30u);
    EXPECT_EQ(snap->timeout.idleSeconds, 600u);
}

TEST(ConfigManagerTest, JsonRoundTrip) {
    auto& cm = ConfigManager::getInstance();
    cm.updateRateLimit([](RateLimitConfig& c) {
        c.globalUpLimit = 1000000;
        c.globalDownLimit = 2000000;
    });
    cm.updateDnsCache([](DnsCacheConfig& c) {
        c.enabled = true;
        c.fixedTtlSeconds = 120;
    });

    const std::string testFile = "test_config_tmp.json";
    cm.saveToFile(testFile);

    std::ifstream ifs(testFile);
    ASSERT_TRUE(ifs.good());
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();
    EXPECT_NE(content.find("1000000"), std::string::npos);
    EXPECT_NE(content.find("120"), std::string::npos);

    cm.updateRateLimit([](RateLimitConfig& c) {
        c.globalUpLimit = 0;
        c.globalDownLimit = 0;
    });
    cm.loadFromFile(testFile);
    auto snap = cm.getSnapshot();
    EXPECT_EQ(snap->rateLimit.globalUpLimit, 1000000u);
    EXPECT_EQ(snap->dnsCache.fixedTtlSeconds, 120u);

    std::remove(testFile.c_str());
}

TEST(ConfigManagerTest, SubscribeCallback) {
    auto& cm = ConfigManager::getInstance();
    int callCount = 0;
    cm.subscribe([&callCount](std::shared_ptr<const Config>) {
        callCount++;
    });
    cm.updateConnectionPool([](ConnectionPoolConfig& c) {
        c.enabled = true;
    });
    EXPECT_GE(callCount, 1);
}

TEST(ConfigManagerTest, LoadNonexistentFileUsesDefaults) {
    auto& cm = ConfigManager::getInstance();
    cm.loadFromFile("nonexistent_file_xyz.json");
    EXPECT_TRUE(cm.lastLoadSuccess());
    auto snap = cm.getSnapshot();
    EXPECT_EQ(snap->ipAcl.mode, AclMode::Disabled);
}

}