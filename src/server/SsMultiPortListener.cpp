#include "SsMultiPortListener.h"
#include "Logger.h"

SsMultiPortListener::SsMultiPortListener(asio::io_context& io)
    : m_io(io) {
}

void SsMultiPortListener::start(const std::vector<Config::SsPortConfig>& ports) {
    for (const auto& portCfg : ports) {
        auto handler = std::make_shared<ShadowsocksHandler>(m_io, portCfg.port, portCfg.passwordHash, portCfg.method);
        handler->start();
        m_handlers.push_back(handler);
    }
    Logger::getInstance().info("[SS] Multi-port listener started with " + std::to_string(ports.size()) + " port(s)");
}

void SsMultiPortListener::stop() {
    for (auto& handler : m_handlers) {
        handler->stop();
    }
    m_handlers.clear();
}