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

// ─── v2.1 加密协议配置 ───

enum class SsMethod {
    AES_256_GCM = 0,
    AES_128_GCM = 1,
    CHACHA20_IETF_POLY1305 = 2
};

struct SsPortConfig {
    uint16_t port = 8388;
    std::string passwordHash;
    std::string salt;
    SsMethod method = SsMethod::AES_256_GCM;
    uint32_t timeoutSeconds = 300;
};

struct ShadowsocksConfig {
    bool enabled = false;
    std::vector<SsPortConfig> ports;
};

struct VmessConfig {
    bool enabled = false;
    uint16_t port = 10086;
    std::vector<std::string> uuids;
    uint8_t alterId = 0;
    uint32_t timeoutSeconds = 300;
};

enum class VlessTransport {
    TCP = 0,
    TLS = 1,
    WEBSOCKET = 2
};

struct VlessConfig {
    bool enabled = false;
    uint16_t port = 10087;
    std::vector<std::string> uuids;
    VlessTransport transport = VlessTransport::TCP;
    uint32_t timeoutSeconds = 300;
};

struct TrojanConfig {
    bool enabled = false;
    uint16_t port = 443;
    std::vector<std::string> passwordHashes;
    std::string tlsCertPath;
    std::string tlsKeyPath;
    uint32_t timeoutSeconds = 300;
};

// ─── v2.1 外网穿透配置 ───

struct RelayNodeConfig {
    std::string nodeId;
    std::string serverHost;
    uint16_t serverPort = 443;
    std::string authTokenHash;
    std::string authTokenSalt;
    bool enabled = true;
    int priority = 0;
    uint32_t reconnectInitialMs = 200;
    uint32_t reconnectMaxMs = 2000;
    uint32_t keepaliveSeconds = 30;
};

struct RelayConfig {
    std::vector<RelayNodeConfig> nodes;
};

enum class NatType {
    UNKNOWN = 0,
    FULL_CONE = 1,
    RESTRICTED = 2,
    PORT_RESTRICTED = 3,
    SYMMETRIC = 4
};

struct NatTraversalConfig {
    bool enabled = false;
    std::string stunServer;
    uint16_t stunPort = 3478;
    uint32_t keepaliveSeconds = 20;
    NatType natType = NatType::UNKNOWN;
    std::string publicIp;
    uint16_t publicPort = 0;
};

// ─── v2.1 基准测试配置 ───

struct BenchmarkConfig {
    uint32_t concurrency = 100;
    uint32_t durationSeconds = 60;
    std::string protocol = "HTTP_CONNECT";
    std::string targetHost = "127.0.0.1";
    uint16_t targetPort = 80;
    uint32_t connectRate = 100;
    uint32_t memoryMonitorInterval = 5;
};

struct Config {
    IpAclConfig ipAcl;
    RateLimitConfig rateLimit;
    ConnectionPoolConfig connectionPool;
    DnsCacheConfig dnsCache;
    TlsConfig tls;
    TimeoutConfig timeout;
    ShadowsocksConfig shadowsocks;
    VmessConfig vmess;
    VlessConfig vless;
    TrojanConfig trojan;
    RelayConfig relay;
    NatTraversalConfig nat;
    BenchmarkConfig benchmark;
};

Config defaultConfig();

bool validate(const Config& cfg);

std::string aclModeToString(AclMode mode);
AclMode stringToAclMode(const std::string& s);

std::string tlsVersionToString(TlsVersion v);
TlsVersion stringToTlsVersion(const std::string& s);

std::string ssMethodToString(SsMethod m);
SsMethod stringToSsMethod(const std::string& s);

std::string vlessTransportToString(VlessTransport t);
VlessTransport stringToVlessTransport(const std::string& s);

std::string natTypeToString(NatType t);
NatType stringToNatType(const std::string& s);

bool isValidUuid(const std::string& s);

}