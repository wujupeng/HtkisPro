#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <vector>
#include <string>
#include "Config.h"

class VmessHandler : public std::enable_shared_from_this<VmessHandler> {
public:
    VmessHandler(asio::io_context& io, uint16_t port,
                 const std::vector<std::string>& uuids, uint8_t alterId);
    void start();
    void stop();

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    asio::io_context& m_io;
    std::vector<std::string> m_uuids;
    uint8_t m_alterId;
    bool m_running{true};
};