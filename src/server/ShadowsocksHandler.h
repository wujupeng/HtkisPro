#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include "Config.h"

class ShadowsocksHandler : public std::enable_shared_from_this<ShadowsocksHandler> {
public:
    ShadowsocksHandler(asio::io_context& io, uint16_t port,
                       const std::string& password, Config::SsMethod method);
    void start();
    void stop();

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    asio::io_context& m_io;
    std::string m_password;
    Config::SsMethod m_method;
    bool m_running{true};
};