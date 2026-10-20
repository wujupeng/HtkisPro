#include "ShadowsocksHandler.h"
#include "ShadowsocksSession.h"
#include "Logger.h"

using asio::ip::tcp;

ShadowsocksHandler::ShadowsocksHandler(asio::io_context& io, uint16_t port,
                                       const std::string& password, Config::SsMethod method)
    : m_acceptor(io, tcp::endpoint(tcp::v4(), port)),
      m_io(io),
      m_password(password),
      m_method(method) {
}

void ShadowsocksHandler::start() {
    Logger::getInstance().info("[SS] Listening on port " + std::to_string(m_acceptor.local_endpoint().port()));
    doAccept();
}

void ShadowsocksHandler::stop() {
    m_running = false;
    std::error_code ec;
    m_acceptor.close(ec);
}

void ShadowsocksHandler::doAccept() {
    auto self = shared_from_this();
    auto socket = std::make_shared<tcp::socket>(m_io);
    m_acceptor.async_accept(*socket,
        [this, self, socket](std::error_code ec) {
            if (!ec && m_running) {
                auto session = std::make_shared<ShadowsocksSession>(std::move(*socket), m_password, m_method);
                session->start();
            }
            if (m_running) {
                doAccept();
            }
        });
}