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