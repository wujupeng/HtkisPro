#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <optional>
#include "RelayTunnelClient.h"
#include "Config.h"

struct NodeStatus {
    std::string nodeId;
    RelayTunnelClient::State state;
    uint64_t bytesUp{0};
    uint64_t bytesDown{0};
    uint32_t activeSessions{0};
    uint64_t latency{0};
};

class RelayManager {
public:
    static RelayManager& getInstance() {
        static RelayManager instance;
        return instance;
    }

    void init(asio::io_context& io, std::shared_ptr<asio::ssl::context> tlsCtx);
    void addNode(const Config::RelayNodeConfig& cfg);
    void removeNode(const std::string& nodeId);
    void enable();
    void disable();
    std::vector<NodeStatus> getAllStatus() const;
    std::optional<std::string> selectByPriority() const;

private:
    RelayManager() = default;

    mutable std::mutex m_mutex;
    asio::io_context* m_io{nullptr};
    std::shared_ptr<asio::ssl::context> m_tlsCtx;
    std::unordered_map<std::string, std::shared_ptr<RelayTunnelClient>> m_nodes;
    std::atomic<bool> m_enabled{false};
};