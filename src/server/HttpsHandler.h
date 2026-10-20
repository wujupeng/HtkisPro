#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <array>
#include <string>
#include "TlsStream.h"

class HttpsHandler : public std::enable_shared_from_this<HttpsHandler> {
public:
    HttpsHandler(asio::ip::tcp::socket socket, std::shared_ptr<asio::ssl::context> tlsCtx);
    void start();

private:
    void do_tls_handshake();
    void do_read_request();
    void do_resolve(const std::string& host, uint16_t port);
    void do_connect(const asio::ip::tcp::resolver::results_type& endpoints);
    void do_stream();
    void do_read_client();
    void do_write_remote(std::size_t length);
    void do_read_remote();
    void do_write_client(std::size_t length);
    void close(const std::string& reason);

    TlsStream m_client_stream;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;
    std::array<uint8_t, 65536> m_client_buffer;
    std::array<uint8_t, 65536> m_remote_buffer;
    std::string m_client_addr;
    std::string m_target_host;
    uint16_t m_target_port{0};
    bool m_tunnel_active{false};
};