#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include "TlsStream.h"
#include "Config.h"

class RelayTunnelClient : public std::enable_shared_from_this<RelayTunnelClient> {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        AUTHENTICATING,
        ACTIVE,
        RECONNECTING
    };

    using DataCallback = std::function<void(const uint8_t*, std::size_t)>;

    RelayTunnelClient(asio::io_context& io,
                      std::shared_ptr<asio::ssl::context> tlsCtx,
                      const Config::RelayNodeConfig& cfg);

    void connect();
    void disconnect();
    State getState() const { return m_state; }
    void sendTunnelData(const uint8_t* data, std::size_t length);
    void setOnDataReceived(DataCallback cb) { m_on_data = std::move(cb); }

private:
    void doResolve();
    void doConnect(const asio::ip::tcp::resolver::results_type& endpoints);
    void doTlsHandshake();
    void doAuth();
    void doReadTunnel();
    void startKeepalive();
    void scheduleReconnect();
    void close();

    asio::io_context& m_io;
    std::shared_ptr<asio::ssl::context> m_tlsCtx;
    Config::RelayNodeConfig m_config;

    std::unique_ptr<TlsStream> m_stream;
    asio::ip::tcp::resolver m_resolver;
    asio::steady_timer m_keepalive_timer;
    asio::steady_timer m_reconnect_timer;

    std::atomic<State> m_state{State::DISCONNECTED};
    DataCallback m_on_data;
    std::vector<uint8_t> m_recv_buffer;
    uint32_t m_reconnect_delay_ms{0};
    std::atomic<bool> m_active{false};
};