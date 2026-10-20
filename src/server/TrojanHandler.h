#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <vector>
#include <string>

class TrojanHandler : public std::enable_shared_from_this<TrojanHandler> {
public:
    TrojanHandler(asio::io_context& io, uint16_t port,
                  std::shared_ptr<asio::ssl::context> tlsCtx,
                  const std::vector<std::string>& passwordHashes);
    void start();
    void stop();

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    asio::io_context& m_io;
    std::shared_ptr<asio::ssl::context> m_tlsCtx;
    std::vector<std::string> m_passwordHashes;
    bool m_running{true};
};