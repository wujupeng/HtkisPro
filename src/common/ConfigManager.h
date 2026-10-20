#pragma once

#include "Config.h"
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>

namespace Config {

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    std::shared_ptr<const Config> getSnapshot() const;

    void updateIpAcl(std::function<void(IpAclConfig&)> mutator);
    void updateRateLimit(std::function<void(RateLimitConfig&)> mutator);
    void updateConnectionPool(std::function<void(ConnectionPoolConfig&)> mutator);
    void updateDnsCache(std::function<void(DnsCacheConfig&)> mutator);
    void updateTls(std::function<void(TlsConfig&)> mutator);
    void updateTimeout(std::function<void(TimeoutConfig&)> mutator);
    void updateShadowsocks(std::function<void(ShadowsocksConfig&)> mutator);
    void updateVmess(std::function<void(VmessConfig&)> mutator);
    void updateVless(std::function<void(VlessConfig&)> mutator);
    void updateTrojan(std::function<void(TrojanConfig&)> mutator);
    void updateRelay(std::function<void(RelayConfig&)> mutator);
    void updateNat(std::function<void(NatTraversalConfig&)> mutator);
    void updateBenchmark(std::function<void(BenchmarkConfig&)> mutator);

    using ConfigCallback = std::function<void(std::shared_ptr<const Config>)>;
    void subscribe(ConfigCallback callback);

    void loadFromFile(const std::string& path);
    void saveToFile(const std::string& path);

    bool lastLoadSuccess() const { return m_loadSuccess; }

private:
    ConfigManager();

    void applyUpdate(std::function<void(Config&)> mutator);
    void notifySubscribers();

    std::shared_ptr<const Config> m_snapshot;
    mutable std::mutex m_mutex;
    std::vector<ConfigCallback> m_subscribers;
    std::atomic<bool> m_loadSuccess{true};
};

}