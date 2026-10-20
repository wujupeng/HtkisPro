#include "RelayConfigStore.h"
#include "ConfigManager.h"
#include "Logger.h"

std::vector<Config::RelayNodeConfig> RelayConfigStore::load() {
    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    return cfg->relay.nodes;
}

bool RelayConfigStore::save(const std::vector<Config::RelayNodeConfig>& nodes) {
    Config::RelayConfig relayCfg;
    relayCfg.nodes = nodes;
    Config::ConfigManager::getInstance().updateRelay(
        [relayCfg](Config::RelayConfig& cfg) { cfg = relayCfg; });
    Logger::getInstance().info("[RELAY] Saved " + std::to_string(nodes.size()) + " node(s)");
    return true;
}