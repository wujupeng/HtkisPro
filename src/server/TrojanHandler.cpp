#include "TrojanHandler.h"
#include "TrojanSession.h"
#include "Logger.h"

using asio::ip::tcp;

TrojanHandler::TrojanHandler(asio::io_context& io, uint16_t port,
                             std::shared_ptr<asio::ssl::context> tlsCtx,
                             const std::vector<std::string>& passwordHashes)
    : m_acceptor(io, tcp::endpoint(tcp::v4(), port)),
      m_io(io),
      m_tlsCtx(tlsCtx),
      m_passwordHashes(passwordHashes) {
}

void TrojanHandler::start() {
    Logger::getInstance().info("[TROJAN] Listening on port " + std::to_string(m_acceptor.local_endpoint().port()));
    doAccept();
}

void TrojanHandler::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
}

void TrojanHandler::doAccept() {
    auto self = shared_from_this();
    auto socket = std::make_shared<tcp::socket>(m_io);
    m_acceptor.async_accept(*socket,
        [this, self, socket](std::error_code ec) {
            if (!ec && m_running) {
                auto session = std::make_shared<TrojanSession>(std::move(*socket), m_tlsCtx, m_passwordHashes);
                session->start();
            }
            if (m_running) {
                doAccept();
            }
        });
}