#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <vector>
#include <string>

class VlessHandler : public std::enable_shared_from_this<VlessHandler> {
public:
    VlessHandler(asio::io_context& io, uint16_t port, const std::vector<std::string>& uuids);
    void start();
    void stop();

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    asio::io_context& m_io;
    std::vector<std::string> m_uuids;
    bool m_running{true};
};