#include "RelayManager.h"
#include "Logger.h"

void RelayManager::init(asio::io_context& io, std::shared_ptr<asio::ssl::context> tlsCtx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_io = &io;
    m_tlsCtx = std::move(tlsCtx);
}

void RelayManager::addNode(const Config::RelayNodeConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_io || !m_tlsCtx) {
        Logger::getInstance().warn("[RELAY] Not initialized, cannot add node " + cfg.nodeId);
        return;
    }
    if (m_nodes.count(cfg.nodeId)) {
        Logger::getInstance().warn("[RELAY] Node " + cfg.nodeId + " already exists");
        return;
    }

    auto client = std::make_shared<RelayTunnelClient>(*m_io, m_tlsCtx, cfg);
    m_nodes[cfg.nodeId] = client;
    if (m_enabled && cfg.enabled) {
        client->connect();
    }
    Logger::getInstance().info("[RELAY] Added node " + cfg.nodeId + " -> " + cfg.serverHost + ":" + std::to_string(cfg.serverPort));
}

void RelayManager::removeNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        it->second->disconnect();
        m_nodes.erase(it);
        Logger::getInstance().info("[RELAY] Removed node " + nodeId);
    }
}

void RelayManager::enable() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = true;
    for (auto& [id, client] : m_nodes) {
        client->connect();
    }
    Logger::getInstance().info("[RELAY] Enabled all nodes");
}

void RelayManager::disable() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = false;
    for (auto& [id, client] : m_nodes) {
        client->disconnect();
    }
    Logger::getInstance().info("[RELAY] Disabled all nodes");
}

std::vector<NodeStatus> RelayManager::getAllStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NodeStatus> result;
    result.reserve(m_nodes.size());
    for (const auto& [id, client] : m_nodes) {
        NodeStatus s;
        s.nodeId = id;
        s.state = client->getState();
        result.push_back(s);
    }
    return result;
}

std::optional<std::string> RelayManager::selectByPriority() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::optional<std::string> best;
    int bestPriority = -1;

    for (const auto& [id, client] : m_nodes) {
        if (client->getState() == RelayTunnelClient::State::ACTIVE) {
            best = id;
            bestPriority = 0;
            break;
        }
    }
    return best;
}