#include "VlessHandler.h"
#include "VlessSession.h"
#include "Logger.h"

using asio::ip::tcp;

VlessHandler::VlessHandler(asio::io_context& io, uint16_t port, const std::vector<std::string>& uuids)
    : m_acceptor(io, tcp::endpoint(tcp::v4(), port)),
      m_io(io),
      m_uuids(uuids) {
}

void VlessHandler::start() {
    Logger::getInstance().info("[VLESS] Listening on port " + std::to_string(m_acceptor.local_endpoint().port()));
    doAccept();
}

void VlessHandler::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
}

void VlessHandler::doAccept() {
    auto self = shared_from_this();
    auto socket = std::make_shared<tcp::socket>(m_io);
    m_acceptor.async_accept(*socket,
        [this, self, socket](std::error_code ec) {
            if (!ec && m_running) {
                auto session = std::make_shared<VlessSession>(std::move(*socket), m_uuids);
                session->start();
            }
            if (m_running) {
                doAccept();
            }
        });
}