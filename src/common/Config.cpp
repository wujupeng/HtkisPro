#include "Config.h"

namespace Config {

Config defaultConfig() {
    Config cfg;
    cfg.ipAcl.mode = AclMode::Disabled;
    cfg.rateLimit.globalUpLimit = 0;
    cfg.rateLimit.globalDownLimit = 0;
    cfg.connectionPool.enabled = false;
    cfg.connectionPool.maxConnections = 256;
    cfg.connectionPool.idleTtlSeconds = 60;
    cfg.connectionPool.crossUserReuse = false;
    cfg.dnsCache.enabled = false;
    cfg.dnsCache.fixedTtlSeconds = 300;
    cfg.dnsCache.maxEntries = 4096;
    cfg.tls.enabled = false;
    cfg.tls.minTlsVersion = TlsVersion::TLS_1_2;
    cfg.timeout.firstPacketSeconds = 10;
    cfg.timeout.connectSeconds = 10;
    cfg.timeout.idleSeconds = 300;
    cfg.timeout.retryCount = 3;
    cfg.timeout.retryInitialBackoffMs = 200;
    cfg.timeout.retryMaxBackoffMs = 2000;
    return cfg;
}

bool validate(const Config& cfg) {
    if (cfg.rateLimit.globalUpLimit > 64ULL * 1024 * 1024 * 1024) return false;
    if (cfg.rateLimit.globalDownLimit > 64ULL * 1024 * 1024 * 1024) return false;
    if (cfg.connectionPool.maxConnections == 0 || cfg.connectionPool.maxConnections > 10000) return false;
    if (cfg.connectionPool.idleTtlSeconds == 0) return false;
    if (cfg.dnsCache.fixedTtlSeconds > 86400) return false;
    if (cfg.dnsCache.maxEntries == 0 || cfg.dnsCache.maxEntries > 100000) return false;
    if (cfg.timeout.firstPacketSeconds == 0) return false;
    if (cfg.timeout.connectSeconds == 0) return false;
    if (cfg.timeout.idleSeconds == 0) return false;
    if (cfg.timeout.retryCount > 10) return false;
    if (cfg.timeout.retryInitialBackoffMs == 0) return false;
    if (cfg.timeout.retryMaxBackoffMs < cfg.timeout.retryInitialBackoffMs) return false;
    for (const auto& [name, limit] : cfg.rateLimit.userLimits) {
        if (limit.upLimit > 64ULL * 1024 * 1024 * 1024) return false;
        if (limit.downLimit > 64ULL * 1024 * 1024 * 1024) return false;
    }
    return true;
}

std::string aclModeToString(AclMode mode) {
    switch (mode) {
        case AclMode::Whitelist: return "whitelist";
        case AclMode::Blacklist: return "blacklist";
        default: return "disabled";
    }
}

AclMode stringToAclMode(const std::string& s) {
    if (s == "whitelist") return AclMode::Whitelist;
    if (s == "blacklist") return AclMode::Blacklist;
    return AclMode::Disabled;
}

std::string tlsVersionToString(TlsVersion v) {
    switch (v) {
        case TlsVersion::TLS_1_0: return "1.0";
        case TlsVersion::TLS_1_1: return "1.1";
        case TlsVersion::TLS_1_2: return "1.2";
        case TlsVersion::TLS_1_3: return "1.3";
        default: return "1.2";
    }
}

TlsVersion stringToTlsVersion(const std::string& s) {
    if (s == "1.0") return TlsVersion::TLS_1_0;
    if (s == "1.1") return TlsVersion::TLS_1_1;
    if (s == "1.3") return TlsVersion::TLS_1_3;
    return TlsVersion::TLS_1_2;
}

}