#include "VmessHandler.h"
#include "VmessSession.h"
#include "Logger.h"

using asio::ip::tcp;

VmessHandler::VmessHandler(asio::io_context& io, uint16_t port,
                           const std::vector<std::string>& uuids, uint8_t alterId)
    : m_acceptor(io, tcp::endpoint(tcp::v4(), port)),
      m_io(io),
      m_uuids(uuids),
      m_alterId(alterId) {
}

void VmessHandler::start() {
    Logger::getInstance().info("[VMESS] Listening on port " + std::to_string(m_acceptor.local_endpoint().port()));
    doAccept();
}

void VmessHandler::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
}

void VmessHandler::doAccept() {
    auto self = shared_from_this();
    auto socket = std::make_shared<tcp::socket>(m_io);
    m_acceptor.async_accept(*socket,
        [this, self, socket](std::error_code ec) {
            if (!ec && m_running) {
                auto session = std::make_shared<VmessSession>(std::move(*socket), m_uuids, m_alterId);
                session->start();
            }
            if (m_running) {
                doAccept();
            }
        });
}