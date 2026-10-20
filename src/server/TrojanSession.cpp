#include "TrojanSession.h"
#include "TrojanHashAuth.h"
#include "Logger.h"
#include "Authenticator.h"
#include "Server.h"
#include <cstring>

using asio::ip::tcp;

TrojanSession::TrojanSession(tcp::socket socket,
                             std::shared_ptr<asio::ssl::context> tlsCtx,
                             const std::vector<std::string>& passwordHashes)
    : m_client_stream(std::move(socket), *tlsCtx),
      m_remote_socket(static_cast<asio::io_context&>(m_client_stream.get_executor().context())),
      m_resolver(static_cast<asio::io_context&>(m_client_stream.get_executor().context())),
      m_passwordHashes(passwordHashes) {
}

void TrojanSession::start() {
    auto ep = m_client_stream.lowest_layer().remote_endpoint();
    m_client_addr = ep.address().to_string() + ":" + std::to_string(ep.port());
    Server::activeSessionCount().fetch_add(1);
    Logger::getInstance().info("[TROJAN] New connection from " + m_client_addr);
    doTlsHandshake();
}

void TrojanSession::doTlsHandshake() {
    auto self = shared_from_this();
    m_client_stream.async_handshake(asio::ssl::stream_base::server,
        [this, self](std::error_code ec) {
            if (!ec) {
                m_tls_handshake_done = true;
                doReadHash();
            } else {
                Logger::getInstance().warn("[TROJAN] TLS handshake failed from " + m_client_addr + ": " + ec.message());
                close("tls handshake failed");
            }
        });
}

void TrojanSession::doReadHash() {
    auto self = shared_from_this();
    m_hash_str.clear();
    m_hash_str.resize(SHA224_HEX_LEN + 2);

    asio::async_read(m_client_stream, asio::buffer(&m_hash_str[0], SHA224_HEX_LEN + 2),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (m_hash_str[SHA224_HEX_LEN] != '\r' || m_hash_str[SHA224_HEX_LEN + 1] != '\n') {
                    Logger::getInstance().warn("[TROJAN] Invalid hash terminator from " + m_client_addr);
                    sendFake503();
                    return;
                }
                std::string hash = m_hash_str.substr(0, SHA224_HEX_LEN);
                if (!TrojanHashAuth::validate(hash, m_passwordHashes)) {
                    Logger::getInstance().warn("[TROJAN] Hash auth failed from " + m_client_addr);
                    Authenticator::getInstance().authenticateTrojanHash(hash, m_client_addr);
                    sendFake503();
                    return;
                }
                Logger::getInstance().info("[TROJAN] Auth success from " + m_client_addr);
                doReadAddress();
            } else {
                close("hash read error");
            }
        });
}

void TrojanSession::doReadAddress() {
    auto self = shared_from_this();
    m_addr_buf.resize(256);
    m_client_stream.async_read_some(asio::buffer(m_addr_buf),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (length < 1) {
                    sendFake503();
                    return;
                }
                uint8_t cmd = m_addr_buf[0];
                if (cmd != 0x01) {
                    Logger::getInstance().warn("[TROJAN] Unsupported command " + std::to_string(cmd) + " from " + m_client_addr);
                    close("unsupported command");
                    return;
                }

                if (length < 2) { close("address too short"); return; }
                uint8_t atyp = m_addr_buf[1];
                std::size_t offset = 2;
                std::string host;

                if (atyp == 0x01) {
                    if (length < offset + 4 + 2) { close("address too short"); return; }
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                             m_addr_buf[offset], m_addr_buf[offset+1],
                             m_addr_buf[offset+2], m_addr_buf[offset+3]);
                    host = buf;
                    offset += 4;
                } else if (atyp == 0x03) {
                    if (length < offset + 1) { close("address too short"); return; }
                    uint8_t dlen = m_addr_buf[offset]; offset += 1;
                    if (length < offset + dlen + 2) { close("address too short"); return; }
                    host = std::string(reinterpret_cast<char*>(&m_addr_buf[offset]), dlen);
                    offset += dlen;
                } else if (atyp == 0x04) {
                    if (length < offset + 16 + 2) { close("address too short"); return; }
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%x:%x:%x:%x:%x:%x:%x:%x",
                             (m_addr_buf[offset]<<8)|m_addr_buf[offset+1],
                             (m_addr_buf[offset+2]<<8)|m_addr_buf[offset+3],
                             (m_addr_buf[offset+4]<<8)|m_addr_buf[offset+5],
                             (m_addr_buf[offset+6]<<8)|m_addr_buf[offset+7],
                             (m_addr_buf[offset+8]<<8)|m_addr_buf[offset+9],
                             (m_addr_buf[offset+10]<<8)|m_addr_buf[offset+11],
                             (m_addr_buf[offset+12]<<8)|m_addr_buf[offset+13],
                             (m_addr_buf[offset+14]<<8)|m_addr_buf[offset+15]);
                    host = buf;
                    offset += 16;
                } else {
                    close("unknown address type");
                    return;
                }

                m_target_host = host;
                m_target_port = (static_cast<uint16_t>(m_addr_buf[offset]) << 8) | m_addr_buf[offset + 1];
                offset += 2;

                Logger::getInstance().info("[TROJAN] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);

                if (offset < length) {
                    std::memcpy(m_client_buffer.data(), m_addr_buf.data() + offset, length - offset);
                }
                doResolve(m_target_host, m_target_port);
            } else {
                close("address read error");
            }
        });
}

void TrojanSession::doResolve(const std::string& host, uint16_t port) {
    auto self = shared_from_this();
    m_resolver.async_resolve(host, std::to_string(port),
        [this, self, host, port](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                doConnect(results);
            } else {
                Logger::getInstance().error("[TROJAN] DNS resolve failed: " + host + " - " + ec.message());
                close("dns failed");
            }
        });
}

void TrojanSession::doConnect(const tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    asio::async_connect(m_remote_socket, endpoints,
        [this, self](std::error_code ec, tcp::endpoint endpoint) {
            if (!ec) {
                m_tunnel_active = true;
                Logger::getInstance().info("[TROJAN] Tunnel established -> " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);
                doStream();
            } else {
                close("connect failed");
            }
        });
}

void TrojanSession::doStream() {
    doReadClient();
    doReadRemote();
}

void TrojanSession::doReadClient() {
    auto self = shared_from_this();
    m_client_stream.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                doWriteRemote(length);
            } else {
                close("client disconnect");
            }
        });
}

void TrojanSession::doWriteRemote(std::size_t length) {
    auto self = shared_from_this();
    asio::async_write(m_remote_socket, asio::buffer(m_client_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadClient();
            } else {
                close("remote write error");
            }
        });
}

void TrojanSession::doReadRemote() {
    auto self = shared_from_this();
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                doWriteClient(length);
            } else {
                close("remote disconnect");
            }
        });
}

void TrojanSession::doWriteClient(std::size_t length) {
    auto self = shared_from_this();
    asio::async_write(m_client_stream, asio::buffer(m_remote_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadRemote();
            } else {
                close("client write error");
            }
        });
}

void TrojanSession::sendFake503() {
    auto self = shared_from_this();
    auto response = std::make_shared<std::string>(
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n");
    asio::async_write(m_client_stream, asio::buffer(*response),
        [this, self, response](std::error_code, std::size_t) {
            close("fake 503");
        });
}

void TrojanSession::close(const std::string& reason) {
    std::error_code ec;
    m_client_stream.lowest_layer().close(ec);
    m_remote_socket.close(ec);
    if (m_tunnel_active) {
        Logger::getInstance().info("[TROJAN] CLOSE " + m_client_addr + " target=" + m_target_host + ":" + std::to_string(m_target_port) + " reason=" + reason);
    } else {
        Logger::getInstance().info("[TROJAN] CLOSE " + m_client_addr + " reason=" + reason);
    }
    m_tunnel_active = false;
    Server::activeSessionCount().fetch_sub(1);
}