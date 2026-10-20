#include "HttpsHandler.h"
#include "TlsContext.h"
#include "Logger.h"
#include <cstring>
#include <sstream>

HttpsHandler::HttpsHandler(asio::ip::tcp::socket socket, std::shared_ptr<asio::ssl::context> tlsCtx)
    : m_client_stream(std::move(socket), *tlsCtx),
      m_remote_socket(m_client_stream.get_executor()),
      m_resolver(m_client_stream.get_executor()) {
    auto ep = m_client_stream.lowest_layer().remote_endpoint();
    m_client_addr = ep.address().to_string() + ":" + std::to_string(ep.port());
}

void HttpsHandler::start() {
    auto self = shared_from_this();
    m_client_stream.lowest_layer().set_option(asio::ip::tcp::no_delay(true));

    auto timer = std::make_shared<asio::steady_timer>(m_client_stream.get_executor());
    timer->expires_after(std::chrono::seconds(10));
    timer->async_wait([self, this](const std::error_code& ec) {
        if (!ec && !m_tunnel_active) {
            close("tls handshake timeout");
        }
    });

    do_tls_handshake();
}

void HttpsHandler::do_tls_handshake() {
    auto self = shared_from_this();
    m_client_stream.async_handshake(asio::ssl::stream_base::server,
        [self, this](const std::error_code& ec) {
            if (ec) {
                close("tls handshake failed: " + ec.message());
                return;
            }
            do_read_request();
        });
}

void HttpsHandler::do_read_request() {
    auto self = shared_from_this();
    m_client_stream.async_read_some(asio::buffer(m_client_buffer),
        [self, this](const std::error_code& ec, std::size_t length) {
            if (ec) {
                close("read request failed: " + ec.message());
                return;
            }

            const char* data = reinterpret_cast<const char*>(m_client_buffer.data());
            if (std::strncmp(data, "CONNECT ", 8) != 0) {
                close("not a CONNECT request");
                return;
            }

            const char* end = reinterpret_cast<const char*>(m_client_buffer.data() + length);
            const char* crlfcrlf = nullptr;
            for (const char* p = data; p < end - 3; ++p) {
                if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') {
                    crlfcrlf = p;
                    break;
                }
            }
            if (!crlfcrlf) {
                close("incomplete CONNECT request");
                return;
            }

            std::string request(data, crlfcrlf);
            std::istringstream iss(request);
            std::string method, target, version;
            iss >> method >> target >> version;

            auto colonPos = target.rfind(':');
            if (colonPos == std::string::npos) {
                close("invalid target format");
                return;
            }
            m_target_host = target.substr(0, colonPos);
            m_target_port = static_cast<uint16_t>(std::stoi(target.substr(colonPos + 1)));

            const char* response = "HTTP/1.1 200 Connection Established\r\n\r\n";
            asio::async_write(m_client_stream, asio::buffer(response, 38),
                [self, this](const std::error_code& ec, std::size_t) {
                    if (ec) {
                        close("write 200 failed: " + ec.message());
                        return;
                    }
                    m_tunnel_active = true;
                    Logger::getInstance().info("[HTTPS] Tunnel established -> " +
                        m_target_host + ":" + std::to_string(m_target_port) +
                        " from=" + m_client_addr);
                    do_stream();
                });
        });
}

void HttpsHandler::do_resolve(const std::string& host, uint16_t port) {
    auto self = shared_from_this();
    m_resolver.async_resolve(host, std::to_string(port),
        [self, this, port](const std::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints) {
            if (ec) {
                close("resolve failed: " + ec.message());
                return;
            }
            do_connect(endpoints);
        });
}

void HttpsHandler::do_connect(const asio::ip::tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    asio::async_connect(m_remote_socket, endpoints,
        [self, this](const std::error_code& ec, const asio::ip::tcp::endpoint&) {
            if (ec) {
                close("connect failed: " + ec.message());
                return;
            }
            m_tunnel_active = true;
            do_stream();
        });
}

void HttpsHandler::do_stream() {
    do_read_client();
    do_read_remote();
}

void HttpsHandler::do_read_client() {
    auto self = shared_from_this();
    m_client_stream.async_read_some(asio::buffer(m_client_buffer),
        [self, this](const std::error_code& ec, std::size_t length) {
            if (ec) {
                close("client read: " + ec.message());
                return;
            }
            do_write_remote(length);
        });
}

void HttpsHandler::do_write_remote(std::size_t length) {
    auto self = shared_from_this();
    asio::async_write(m_remote_socket, asio::buffer(m_client_buffer, length),
        [self, this](const std::error_code& ec, std::size_t) {
            if (ec) {
                close("remote write: " + ec.message());
                return;
            }
            do_read_client();
        });
}

void HttpsHandler::do_read_remote() {
    auto self = shared_from_this();
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [self, this](const std::error_code& ec, std::size_t length) {
            if (ec) {
                close("remote read: " + ec.message());
                return;
            }
            do_write_client(length);
        });
}

void HttpsHandler::do_write_client(std::size_t length) {
    auto self = shared_from_this();
    asio::async_write(m_client_stream, asio::buffer(m_remote_buffer, length),
        [self, this](const std::error_code& ec, std::size_t) {
            if (ec) {
                close("client write: " + ec.message());
                return;
            }
            do_read_remote();
        });
}

void HttpsHandler::close(const std::string& reason) {
    if (m_tunnel_active) {
        m_tunnel_active = false;
        Logger::getInstance().info("[HTTPS] Close " + m_client_addr +
            " target=" + m_target_host + ":" + std::to_string(m_target_port) +
            " reason=" + reason);
    }

    std::error_code ec;
    m_client_stream.lowest_layer().close(ec);
    m_remote_socket.close(ec);
}