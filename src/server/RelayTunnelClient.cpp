#include "RelayTunnelClient.h"
#include "Logger.h"
#include "ReconnectPolicy.h"
#include <cstring>

using asio::ip::tcp;

RelayTunnelClient::RelayTunnelClient(asio::io_context& io,
                                     std::shared_ptr<asio::ssl::context> tlsCtx,
                                     const Config::RelayNodeConfig& cfg)
    : m_io(io),
      m_tlsCtx(std::move(tlsCtx)),
      m_config(cfg),
      m_resolver(io),
      m_keepalive_timer(io),
      m_reconnect_timer(io),
      m_recv_buffer(65536) {
}

void RelayTunnelClient::connect() {
    if (m_state != State::DISCONNECTED && m_state != State::RECONNECTING) return;
    m_active = true;
    m_state = State::CONNECTING;
    Logger::getInstance().info("[RELAY] Connecting to " + m_config.serverHost + ":" + std::to_string(m_config.serverPort) + " nodeId=" + m_config.nodeId);
    doResolve();
}

void RelayTunnelClient::doResolve() {
    auto self = shared_from_this();
    m_resolver.async_resolve(m_config.serverHost, std::to_string(m_config.serverPort),
        [this, self](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                doConnect(results);
            } else {
                Logger::getInstance().error("[RELAY] Resolve failed: " + m_config.serverHost + " - " + ec.message());
                scheduleReconnect();
            }
        });
}

void RelayTunnelClient::doConnect(const tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    auto socket = std::make_shared<tcp::socket>(m_io);
    asio::async_connect(*socket, endpoints,
        [this, self, socket](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                m_stream = std::make_unique<TlsStream>(std::move(*socket), *m_tlsCtx);
                doTlsHandshake();
            } else {
                Logger::getInstance().error("[RELAY] Connect failed: " + ec.message());
                scheduleReconnect();
            }
        });
}

void RelayTunnelClient::doTlsHandshake() {
    auto self = shared_from_this();
    m_state = State::CONNECTING;
    m_stream->async_handshake(asio::ssl::stream_base::client,
        [this, self](std::error_code ec) {
            if (!ec) {
                Logger::getInstance().info("[RELAY] TLS handshake success nodeId=" + m_config.nodeId);
                doAuth();
            } else {
                Logger::getInstance().error("[RELAY] TLS handshake failed: " + ec.message());
                scheduleReconnect();
            }
        });
}

void RelayTunnelClient::doAuth() {
    auto self = shared_from_this();
    m_state = State::AUTHENTICATING;

    std::vector<uint8_t> authMsg;
    authMsg.push_back(0x01);
    uint16_t nodeIdLen = static_cast<uint16_t>(m_config.nodeId.size());
    authMsg.push_back(static_cast<uint8_t>(nodeIdLen >> 8));
    authMsg.push_back(static_cast<uint8_t>(nodeIdLen & 0xFF));
    authMsg.insert(authMsg.end(), m_config.nodeId.begin(), m_config.nodeId.end());
    authMsg.insert(authMsg.end(), m_config.authTokenHash.begin(), m_config.authTokenHash.end());

    asio::async_write(*m_stream, asio::buffer(authMsg),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                asio::async_read(*m_stream, asio::buffer(m_recv_buffer.data(), 1),
                    [this, self](std::error_code ec2, std::size_t) {
                        if (!ec2 && m_recv_buffer[0] == 0x01) {
                            m_state = State::ACTIVE;
                            Logger::getInstance().info("[RELAY] Auth success nodeId=" + m_config.nodeId);
                            startKeepalive();
                            doReadTunnel();
                        } else {
                            Logger::getInstance().error("[RELAY] Auth failed nodeId=" + m_config.nodeId);
                            m_state = State::DISCONNECTED;
                            close();
                        }
                    });
            } else {
                scheduleReconnect();
            }
        });
}

void RelayTunnelClient::doReadTunnel() {
    auto self = shared_from_this();
    m_stream->async_read_some(asio::buffer(m_recv_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (m_on_data) {
                    m_on_data(m_recv_buffer.data(), length);
                }
                doReadTunnel();
            } else {
                Logger::getInstance().warn("[RELAY] Tunnel disconnected nodeId=" + m_config.nodeId + " - " + ec.message());
                scheduleReconnect();
            }
        });
}

void RelayTunnelClient::startKeepalive() {
    auto self = shared_from_this();
    m_keepalive_timer.expires_after(std::chrono::seconds(m_config.keepaliveSeconds));
    m_keepalive_timer.async_wait(
        [this, self](std::error_code ec) {
            if (!ec && m_state == State::ACTIVE && m_stream) {
                uint8_t heartbeat = 0x00;
                asio::async_write(*m_stream, asio::buffer(&heartbeat, 1),
                    [this, self](std::error_code, std::size_t) {
                        if (m_state == State::ACTIVE) {
                            startKeepalive();
                        }
                    });
            }
        });
}

void RelayTunnelClient::scheduleReconnect() {
    if (!m_active) return;
    m_state = State::RECONNECTING;
    close();

    if (m_reconnect_delay_ms == 0) {
        m_reconnect_delay_ms = m_config.reconnectInitialMs;
    } else {
        m_reconnect_delay_ms = (std::min)(m_reconnect_delay_ms * 2, m_config.reconnectMaxMs);
    }

    Logger::getInstance().info("[RELAY] Reconnecting in " + std::to_string(m_reconnect_delay_ms) + "ms nodeId=" + m_config.nodeId);

    auto self = shared_from_this();
    m_reconnect_timer.expires_after(std::chrono::milliseconds(m_reconnect_delay_ms));
    m_reconnect_timer.async_wait(
        [this, self](std::error_code ec) {
            if (!ec && m_active) {
                connect();
            }
        });
}

void RelayTunnelClient::disconnect() {
    m_active = false;
    m_state = State::DISCONNECTED;
    close();
}

void RelayTunnelClient::close() {
    m_keepalive_timer.cancel();
    m_reconnect_timer.cancel();
    if (m_stream) {
        std::error_code ec;
        m_stream->lowest_layer().close(ec);
        m_stream.reset();
    }
}

void RelayTunnelClient::sendTunnelData(const uint8_t* data, std::size_t length) {
    if (m_state != State::ACTIVE || !m_stream) return;
    auto self = shared_from_this();
    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + length);
    asio::async_write(*m_stream, asio::buffer(*buf),
        [this, self, buf](std::error_code ec, std::size_t) {
            if (ec) {
                scheduleReconnect();
            }
        });
}