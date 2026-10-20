#include "ConfigManager.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Config {

using json = nlohmann::json;

ConfigManager::ConfigManager()
    : m_snapshot(std::make_shared<const Config>(defaultConfig())) {
}

std::shared_ptr<const Config> ConfigManager::getSnapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

void ConfigManager::applyUpdate(std::function<void(Config&)> mutator) {
    std::vector<ConfigCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Config newConfig = *m_snapshot;
        mutator(newConfig);
        if (!validate(newConfig)) {
            Logger::getInstance().warn("[CONFIG] Validation failed, keeping previous config");
            return;
        }
        m_snapshot = std::make_shared<const Config>(std::move(newConfig));
        callbacks = m_subscribers;
    }
    for (const auto& cb : callbacks) {
        cb(m_snapshot);
    }
}

void ConfigManager::notifySubscribers() {
}

void ConfigManager::updateIpAcl(std::function<void(IpAclConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.ipAcl); });
}

void ConfigManager::updateRateLimit(std::function<void(RateLimitConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.rateLimit); });
}

void ConfigManager::updateConnectionPool(std::function<void(ConnectionPoolConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.connectionPool); });
}

void ConfigManager::updateDnsCache(std::function<void(DnsCacheConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.dnsCache); });
}

void ConfigManager::updateTls(std::function<void(TlsConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.tls); });
}

void ConfigManager::updateTimeout(std::function<void(TimeoutConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.timeout); });
}

void ConfigManager::updateShadowsocks(std::function<void(ShadowsocksConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.shadowsocks); });
}

void ConfigManager::updateVmess(std::function<void(VmessConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.vmess); });
}

void ConfigManager::updateVless(std::function<void(VlessConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.vless); });
}

void ConfigManager::updateTrojan(std::function<void(TrojanConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.trojan); });
}

void ConfigManager::updateRelay(std::function<void(RelayConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.relay); });
}

void ConfigManager::updateNat(std::function<void(NatTraversalConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.nat); });
}

void ConfigManager::updateBenchmark(std::function<void(BenchmarkConfig&)> mutator) {
    applyUpdate([&mutator](Config& cfg) { mutator(cfg.benchmark); });
}

void ConfigManager::subscribe(ConfigCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers.push_back(std::move(callback));
}

static json serializeIpAcl(const IpAclConfig& cfg) {
    json j;
    j["mode"] = aclModeToString(cfg.mode);
    json rules = json::array();
    for (const auto& r : cfg.rules) {
        rules.push_back({{"network", r.network}, {"isIpv6", r.isIpv6}});
    }
    j["rules"] = rules;
    return j;
}

static json serializeRateLimit(const RateLimitConfig& cfg) {
    json j;
    j["globalUpLimit"] = cfg.globalUpLimit;
    j["globalDownLimit"] = cfg.globalDownLimit;
    json users = json::object();
    for (const auto& [name, limit] : cfg.userLimits) {
        users[name] = {{"upLimit", limit.upLimit}, {"downLimit", limit.downLimit}, {"priority", limit.priority}};
    }
    j["userLimits"] = users;
    return j;
}

static json serializeConnectionPool(const ConnectionPoolConfig& cfg) {
    return {
        {"enabled", cfg.enabled},
        {"maxConnections", cfg.maxConnections},
        {"idleTtlSeconds", cfg.idleTtlSeconds},
        {"crossUserReuse", cfg.crossUserReuse}
    };
}

static json serializeDnsCache(const DnsCacheConfig& cfg) {
    return {
        {"enabled", cfg.enabled},
        {"fixedTtlSeconds", cfg.fixedTtlSeconds},
        {"maxEntries", cfg.maxEntries},
        {"backupDnsServers", cfg.backupDnsServers}
    };
}

static json serializeTls(const TlsConfig& cfg) {
    return {
        {"enabled", cfg.enabled},
        {"certFilePath", cfg.certFilePath},
        {"keyFilePath", cfg.keyFilePath},
        {"minTlsVersion", tlsVersionToString(cfg.minTlsVersion)}
    };
}

static json serializeTimeout(const TimeoutConfig& cfg) {
    return {
        {"firstPacketSeconds", cfg.firstPacketSeconds},
        {"connectSeconds", cfg.connectSeconds},
        {"idleSeconds", cfg.idleSeconds},
        {"retryCount", cfg.retryCount},
        {"retryInitialBackoffMs", cfg.retryInitialBackoffMs},
        {"retryMaxBackoffMs", cfg.retryMaxBackoffMs}
    };
}

static json serializeShadowsocks(const ShadowsocksConfig& cfg) {
    json j;
    j["enabled"] = cfg.enabled;
    json ports = json::array();
    for (const auto& p : cfg.ports) {
        ports.push_back({
            {"port", p.port}, {"passwordHash", p.passwordHash}, {"salt", p.salt},
            {"method", ssMethodToString(p.method)}, {"timeoutSeconds", p.timeoutSeconds}
        });
    }
    j["ports"] = ports;
    return j;
}

static json serializeVmess(const VmessConfig& cfg) {
    return {{"enabled", cfg.enabled}, {"port", cfg.port}, {"uuids", cfg.uuids},
            {"alterId", cfg.alterId}, {"timeoutSeconds", cfg.timeoutSeconds}};
}

static json serializeVless(const VlessConfig& cfg) {
    return {{"enabled", cfg.enabled}, {"port", cfg.port}, {"uuids", cfg.uuids},
            {"transport", vlessTransportToString(cfg.transport)}, {"timeoutSeconds", cfg.timeoutSeconds}};
}

static json serializeTrojan(const TrojanConfig& cfg) {
    return {{"enabled", cfg.enabled}, {"port", cfg.port}, {"passwordHashes", cfg.passwordHashes},
            {"tlsCertPath", cfg.tlsCertPath}, {"tlsKeyPath", cfg.tlsKeyPath},
            {"timeoutSeconds", cfg.timeoutSeconds}};
}

static json serializeRelay(const RelayConfig& cfg) {
    json nodes = json::array();
    for (const auto& n : cfg.nodes) {
        nodes.push_back({
            {"nodeId", n.nodeId}, {"serverHost", n.serverHost}, {"serverPort", n.serverPort},
            {"authTokenHash", n.authTokenHash}, {"authTokenSalt", n.authTokenSalt},
            {"enabled", n.enabled}, {"priority", n.priority},
            {"reconnectInitialMs", n.reconnectInitialMs}, {"reconnectMaxMs", n.reconnectMaxMs},
            {"keepaliveSeconds", n.keepaliveSeconds}
        });
    }
    return {{"nodes", nodes}};
}

static json serializeNat(const NatTraversalConfig& cfg) {
    return {{"enabled", cfg.enabled}, {"stunServer", cfg.stunServer}, {"stunPort", cfg.stunPort},
            {"keepaliveSeconds", cfg.keepaliveSeconds}, {"natType", natTypeToString(cfg.natType)},
            {"publicIp", cfg.publicIp}, {"publicPort", cfg.publicPort}};
}

static json serializeBenchmark(const BenchmarkConfig& cfg) {
    return {{"concurrency", cfg.concurrency}, {"durationSeconds", cfg.durationSeconds},
            {"protocol", cfg.protocol}, {"targetHost", cfg.targetHost}, {"targetPort", cfg.targetPort},
            {"connectRate", cfg.connectRate}, {"memoryMonitorInterval", cfg.memoryMonitorInterval}};
}

void ConfigManager::saveToFile(const std::string& path) {
    std::shared_ptr<const Config> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot = m_snapshot;
    }

    json j;
    j["ipAcl"] = serializeIpAcl(snapshot->ipAcl);
    j["rateLimit"] = serializeRateLimit(snapshot->rateLimit);
    j["connectionPool"] = serializeConnectionPool(snapshot->connectionPool);
    j["dnsCache"] = serializeDnsCache(snapshot->dnsCache);
    j["tls"] = serializeTls(snapshot->tls);
    j["timeout"] = serializeTimeout(snapshot->timeout);
    j["shadowsocks"] = serializeShadowsocks(snapshot->shadowsocks);
    j["vmess"] = serializeVmess(snapshot->vmess);
    j["vless"] = serializeVless(snapshot->vless);
    j["trojan"] = serializeTrojan(snapshot->trojan);
    j["relay"] = serializeRelay(snapshot->relay);
    j["nat"] = serializeNat(snapshot->nat);
    j["benchmark"] = serializeBenchmark(snapshot->benchmark);

    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::getInstance().error("[CONFIG] Failed to save config to " + path);
        return;
    }
    file << j.dump(2);
    Logger::getInstance().info("[CONFIG] Config saved to " + path);
}

static IpAclConfig parseIpAcl(const json& j) {
    IpAclConfig cfg;
    if (j.contains("mode")) cfg.mode = stringToAclMode(j["mode"].get<std::string>());
    if (j.contains("rules")) {
        for (const auto& r : j["rules"]) {
            CidrRule rule;
            rule.network = r.value("network", "");
            rule.isIpv6 = r.value("isIpv6", false);
            if (!rule.network.empty()) cfg.rules.push_back(rule);
        }
    }
    return cfg;
}

static RateLimitConfig parseRateLimit(const json& j) {
    RateLimitConfig cfg;
    cfg.globalUpLimit = j.value("globalUpLimit", 0ULL);
    cfg.globalDownLimit = j.value("globalDownLimit", 0ULL);
    if (j.contains("userLimits")) {
        for (auto it = j["userLimits"].begin(); it != j["userLimits"].end(); ++it) {
            UserRateLimit limit;
            limit.upLimit = it->value("upLimit", 0ULL);
            limit.downLimit = it->value("downLimit", 0ULL);
            limit.priority = it->value("priority", 0);
            cfg.userLimits[it.key()] = limit;
        }
    }
    return cfg;
}

static ConnectionPoolConfig parseConnectionPool(const json& j) {
    ConnectionPoolConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.maxConnections = j.value("maxConnections", 256u);
    cfg.idleTtlSeconds = j.value("idleTtlSeconds", 60u);
    cfg.crossUserReuse = j.value("crossUserReuse", false);
    return cfg;
}

static DnsCacheConfig parseDnsCache(const json& j) {
    DnsCacheConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.fixedTtlSeconds = j.value("fixedTtlSeconds", 300u);
    cfg.maxEntries = j.value("maxEntries", 4096u);
    if (j.contains("backupDnsServers")) {
        cfg.backupDnsServers = j["backupDnsServers"].get<std::vector<std::string>>();
    }
    return cfg;
}

static TlsConfig parseTls(const json& j) {
    TlsConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.certFilePath = j.value("certFilePath", "");
    cfg.keyFilePath = j.value("keyFilePath", "");
    if (j.contains("minTlsVersion")) cfg.minTlsVersion = stringToTlsVersion(j["minTlsVersion"].get<std::string>());
    return cfg;
}

static TimeoutConfig parseTimeout(const json& j) {
    TimeoutConfig cfg;
    cfg.firstPacketSeconds = j.value("firstPacketSeconds", 10u);
    cfg.connectSeconds = j.value("connectSeconds", 10u);
    cfg.idleSeconds = j.value("idleSeconds", 300u);
    cfg.retryCount = j.value("retryCount", 3u);
    cfg.retryInitialBackoffMs = j.value("retryInitialBackoffMs", 200u);
    cfg.retryMaxBackoffMs = j.value("retryMaxBackoffMs", 2000u);
    return cfg;
}

static ShadowsocksConfig parseShadowsocks(const json& j) {
    ShadowsocksConfig cfg;
    cfg.enabled = j.value("enabled", false);
    if (j.contains("ports")) {
        for (const auto& p : j["ports"]) {
            SsPortConfig sp;
            sp.port = p.value("port", 8388u);
            sp.passwordHash = p.value("passwordHash", "");
            sp.salt = p.value("salt", "");
            sp.method = stringToSsMethod(p.value("method", "aes-256-gcm"));
            sp.timeoutSeconds = p.value("timeoutSeconds", 300u);
            cfg.ports.push_back(sp);
        }
    }
    return cfg;
}

static VmessConfig parseVmess(const json& j) {
    VmessConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.port = j.value("port", 10086u);
    if (j.contains("uuids")) cfg.uuids = j["uuids"].get<std::vector<std::string>>();
    cfg.alterId = j.value("alterId", 0);
    cfg.timeoutSeconds = j.value("timeoutSeconds", 300u);
    return cfg;
}

static VlessConfig parseVless(const json& j) {
    VlessConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.port = j.value("port", 10087u);
    if (j.contains("uuids")) cfg.uuids = j["uuids"].get<std::vector<std::string>>();
    cfg.transport = stringToVlessTransport(j.value("transport", "tcp"));
    cfg.timeoutSeconds = j.value("timeoutSeconds", 300u);
    return cfg;
}

static TrojanConfig parseTrojan(const json& j) {
    TrojanConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.port = j.value("port", 443u);
    if (j.contains("passwordHashes")) cfg.passwordHashes = j["passwordHashes"].get<std::vector<std::string>>();
    cfg.tlsCertPath = j.value("tlsCertPath", "");
    cfg.tlsKeyPath = j.value("tlsKeyPath", "");
    cfg.timeoutSeconds = j.value("timeoutSeconds", 300u);
    return cfg;
}

static RelayConfig parseRelay(const json& j) {
    RelayConfig cfg;
    if (j.contains("nodes")) {
        for (const auto& n : j["nodes"]) {
            RelayNodeConfig node;
            node.nodeId = n.value("nodeId", "");
            node.serverHost = n.value("serverHost", "");
            node.serverPort = n.value("serverPort", 443u);
            node.authTokenHash = n.value("authTokenHash", "");
            node.authTokenSalt = n.value("authTokenSalt", "");
            node.enabled = n.value("enabled", true);
            node.priority = n.value("priority", 0);
            node.reconnectInitialMs = n.value("reconnectInitialMs", 200u);
            node.reconnectMaxMs = n.value("reconnectMaxMs", 2000u);
            node.keepaliveSeconds = n.value("keepaliveSeconds", 30u);
            cfg.nodes.push_back(node);
        }
    }
    return cfg;
}

static NatTraversalConfig parseNat(const json& j) {
    NatTraversalConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.stunServer = j.value("stunServer", "");
    cfg.stunPort = j.value("stunPort", 3478u);
    cfg.keepaliveSeconds = j.value("keepaliveSeconds", 20u);
    cfg.natType = stringToNatType(j.value("natType", "unknown"));
    cfg.publicIp = j.value("publicIp", "");
    cfg.publicPort = j.value("publicPort", 0u);
    return cfg;
}

static BenchmarkConfig parseBenchmark(const json& j) {
    BenchmarkConfig cfg;
    cfg.concurrency = j.value("concurrency", 100u);
    cfg.durationSeconds = j.value("durationSeconds", 60u);
    cfg.protocol = j.value("protocol", "HTTP_CONNECT");
    cfg.targetHost = j.value("targetHost", "127.0.0.1");
    cfg.targetPort = j.value("targetPort", 80u);
    cfg.connectRate = j.value("connectRate", 100u);
    cfg.memoryMonitorInterval = j.value("memoryMonitorInterval", 5u);
    return cfg;
}

void ConfigManager::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::getInstance().info("[CONFIG] No config file at " + path + ", using defaults");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot = std::make_shared<const Config>(defaultConfig());
        m_loadSuccess = true;
        return;
    }

    try {
        json j;
        file >> j;

        Config cfg;
        if (j.contains("ipAcl")) cfg.ipAcl = parseIpAcl(j["ipAcl"]);
        if (j.contains("rateLimit")) cfg.rateLimit = parseRateLimit(j["rateLimit"]);
        if (j.contains("connectionPool")) cfg.connectionPool = parseConnectionPool(j["connectionPool"]);
        if (j.contains("dnsCache")) cfg.dnsCache = parseDnsCache(j["dnsCache"]);
        if (j.contains("tls")) cfg.tls = parseTls(j["tls"]);
        if (j.contains("timeout")) cfg.timeout = parseTimeout(j["timeout"]);
        if (j.contains("shadowsocks")) cfg.shadowsocks = parseShadowsocks(j["shadowsocks"]);
        if (j.contains("vmess")) cfg.vmess = parseVmess(j["vmess"]);
        if (j.contains("vless")) cfg.vless = parseVless(j["vless"]);
        if (j.contains("trojan")) cfg.trojan = parseTrojan(j["trojan"]);
        if (j.contains("relay")) cfg.relay = parseRelay(j["relay"]);
        if (j.contains("nat")) cfg.nat = parseNat(j["nat"]);
        if (j.contains("benchmark")) cfg.benchmark = parseBenchmark(j["benchmark"]);

        if (!validate(cfg)) {
            Logger::getInstance().error("[CONFIG] Config validation failed, using defaults");
            std::lock_guard<std::mutex> lock(m_mutex);
            m_snapshot = std::make_shared<const Config>(defaultConfig());
            m_loadSuccess = false;
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot = std::make_shared<const Config>(std::move(cfg));
        m_loadSuccess = true;
        Logger::getInstance().info("[CONFIG] Config loaded from " + path);
    } catch (const std::exception& e) {
        Logger::getInstance().error(std::string("[CONFIG] Failed to parse config: ") + e.what());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshot = std::make_shared<const Config>(defaultConfig());
        m_loadSuccess = false;
    }
}

}