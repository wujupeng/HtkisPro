#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <ctime>

namespace Config {

enum class AclMode {
    Disabled = 0,
    Whitelist = 1,
    Blacklist = 2
};

enum class TlsVersion {
    TLS_1_0 = 0,
    TLS_1_1 = 1,
    TLS_1_2 = 2,
    TLS_1_3 = 3
};

struct CidrRule {
    std::string network;
    bool isIpv6 = false;
};

struct IpAclConfig {
    AclMode mode = AclMode::Disabled;
    std::vector<CidrRule> rules;
};

struct UserRateLimit {
    uint64_t upLimit = 0;
    uint64_t downLimit = 0;
    int priority = 0;
};

struct RateLimitConfig {
    uint64_t globalUpLimit = 0;
    uint64_t globalDownLimit = 0;
    std::map<std::string, UserRateLimit> userLimits;
};

struct ConnectionPoolConfig {
    bool enabled = false;
    uint32_t maxConnections = 256;
    uint32_t idleTtlSeconds = 60;
    bool crossUserReuse = false;
};

struct DnsCacheConfig {
    bool enabled = false;
    uint32_t fixedTtlSeconds = 300;
    uint32_t maxEntries = 4096;
    std::vector<std::string> backupDnsServers;
};

struct TlsConfig {
    bool enabled = false;
    std::string certFilePath;
    std::string keyFilePath;
    TlsVersion minTlsVersion = TlsVersion::TLS_1_2;
};

struct TimeoutConfig {
    uint32_t firstPacketSeconds = 10;
    uint32_t connectSeconds = 10;
    uint32_t idleSeconds = 300;
    uint32_t retryCount = 3;
    uint32_t retryInitialBackoffMs = 200;
    uint32_t retryMaxBackoffMs = 2000;
};

struct Config {
    IpAclConfig ipAcl;
    RateLimitConfig rateLimit;
    ConnectionPoolConfig connectionPool;
    DnsCacheConfig dnsCache;
    TlsConfig tls;
    TimeoutConfig timeout;
};

Config defaultConfig();

bool validate(const Config& cfg);

std::string aclModeToString(AclMode mode);
AclMode stringToAclMode(const std::string& s);

std::string tlsVersionToString(TlsVersion v);
TlsVersion stringToTlsVersion(const std::string& s);

}