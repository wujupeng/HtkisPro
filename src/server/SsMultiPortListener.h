#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <vector>
#include "ShadowsocksHandler.h"
#include "Config.h"

class SsMultiPortListener : public std::enable_shared_from_this<SsMultiPortListener> {
public:
    explicit SsMultiPortListener(asio::io_context& io);
    void start(const std::vector<Config::SsPortConfig>& ports);
    void stop();

private:
    asio::io_context& m_io;
    std::vector<std::shared_ptr<ShadowsocksHandler>> m_handlers;
};