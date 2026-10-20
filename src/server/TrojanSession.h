#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include "TlsStream.h"

class TrojanSession : public std::enable_shared_from_this<TrojanSession> {
public:
    TrojanSession(asio::ip::tcp::socket socket,
                  std::shared_ptr<asio::ssl::context> tlsCtx,
                  const std::vector<std::string>& passwordHashes);

    void start();

private:
    void doTlsHandshake();
    void doReadHash();
    void doReadAddress();
    void doResolve(const std::string& host, uint16_t port);
    void doConnect(const asio::ip::tcp::resolver::results_type& endpoints);
    void doStream();
    void doReadClient();
    void doWriteRemote(std::size_t length);
    void doReadRemote();
    void doWriteClient(std::size_t length);
    void sendFake503();
    void close(const std::string& reason);

    TlsStream m_client_stream;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;

    std::array<uint8_t, 65536> m_client_buffer;
    std::array<uint8_t, 65536> m_remote_buffer;

    std::vector<std::string> m_passwordHashes;
    std::string m_client_addr;
    std::string m_target_host;
    uint16_t m_target_port{0};
    std::string m_hash_str;
    std::vector<uint8_t> m_addr_buf;
    bool m_tunnel_active{false};
    bool m_tls_handshake_done{false};

    static constexpr std::size_t SHA224_HEX_LEN = 56;
};