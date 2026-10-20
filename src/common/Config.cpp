#include "Config.h"
#include <regex>

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
    cfg.shadowsocks.enabled = false;
    cfg.vmess.enabled = false;
    cfg.vmess.port = 10086;
    cfg.vmess.alterId = 0;
    cfg.vless.enabled = false;
    cfg.vless.port = 10087;
    cfg.vless.transport = VlessTransport::TCP;
    cfg.trojan.enabled = false;
    cfg.trojan.port = 443;
    cfg.relay.nodes.clear();
    cfg.nat.enabled = false;
    cfg.nat.stunPort = 3478;
    cfg.nat.keepaliveSeconds = 20;
    cfg.nat.natType = NatType::UNKNOWN;
    cfg.benchmark.concurrency = 100;
    cfg.benchmark.durationSeconds = 60;
    cfg.benchmark.connectRate = 100;
    cfg.benchmark.memoryMonitorInterval = 5;
    return cfg;
}

bool isValidUuid(const std::string& s) {
    static const std::regex uuidRegex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(s, uuidRegex);
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
    if (cfg.shadowsocks.enabled) {
        for (const auto& p : cfg.shadowsocks.ports) {
            if (p.port == 0) return false;
            if (p.timeoutSeconds == 0) return false;
        }
    }
    if (cfg.vmess.enabled) {
        if (cfg.vmess.port == 0) return false;
        if (cfg.vmess.alterId > 16) return false;
        for (const auto& uuid : cfg.vmess.uuids) {
            if (!isValidUuid(uuid)) return false;
        }
    }
    if (cfg.vless.enabled) {
        if (cfg.vless.port == 0) return false;
        for (const auto& uuid : cfg.vless.uuids) {
            if (!isValidUuid(uuid)) return false;
        }
    }
    if (cfg.trojan.enabled) {
        if (cfg.trojan.port == 0) return false;
        if (cfg.trojan.passwordHashes.empty()) return false;
    }
    for (const auto& node : cfg.relay.nodes) {
        if (node.serverHost.empty()) return false;
        if (node.serverPort == 0) return false;
        if (node.keepaliveSeconds == 0) return false;
    }
    if (cfg.nat.enabled && cfg.nat.stunServer.empty()) return false;
    if (cfg.benchmark.concurrency == 0) return false;
    if (cfg.benchmark.durationSeconds == 0) return false;
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

std::string ssMethodToString(SsMethod m) {
    switch (m) {
        case SsMethod::AES_256_GCM: return "aes-256-gcm";
        case SsMethod::AES_128_GCM: return "aes-128-gcm";
        case SsMethod::CHACHA20_IETF_POLY1305: return "chacha20-ietf-poly1305";
        default: return "aes-256-gcm";
    }
}

SsMethod stringToSsMethod(const std::string& s) {
    if (s == "aes-128-gcm") return SsMethod::AES_128_GCM;
    if (s == "chacha20-ietf-poly1305") return SsMethod::CHACHA20_IETF_POLY1305;
    return SsMethod::AES_256_GCM;
}

std::string vlessTransportToString(VlessTransport t) {
    switch (t) {
        case VlessTransport::TCP: return "tcp";
        case VlessTransport::TLS: return "tls";
        case VlessTransport::WEBSOCKET: return "websocket";
        default: return "tcp";
    }
}

VlessTransport stringToVlessTransport(const std::string& s) {
    if (s == "tls") return VlessTransport::TLS;
    if (s == "websocket") return VlessTransport::WEBSOCKET;
    return VlessTransport::TCP;
}

std::string natTypeToString(NatType t) {
    switch (t) {
        case NatType::FULL_CONE: return "full_cone";
        case NatType::RESTRICTED: return "restricted";
        case NatType::PORT_RESTRICTED: return "port_restricted";
        case NatType::SYMMETRIC: return "symmetric";
        default: return "unknown";
    }
}

NatType stringToNatType(const std::string& s) {
    if (s == "full_cone") return NatType::FULL_CONE;
    if (s == "restricted") return NatType::RESTRICTED;
    if (s == "port_restricted") return NatType::PORT_RESTRICTED;
    if (s == "symmetric") return NatType::SYMMETRIC;
    return NatType::UNKNOWN;
}

}