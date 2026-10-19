#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <array>
#include <string>
#include <atomic>

class UdpRelaySession : public std::enable_shared_from_this<UdpRelaySession> {
public:
    UdpRelaySession(asio::io_context& io, asio::ip::tcp::socket& controlSocket);
    ~UdpRelaySession();

    void start();
    void stop();

    asio::ip::udp::endpoint getBoundEndpoint() const { return m_boundEndpoint; }

private:
    void doReceive();
    void doReceiveFromTarget();
    void handleClientUdp(std::size_t length, asio::ip::udp::endpoint sender);
    void handleTargetUdp(std::size_t length);

    asio::io_context& m_io;
    asio::ip::tcp::socket& m_controlSocket;
    asio::ip::udp::socket m_udpSocket;
    asio::ip::udp::socket m_targetUdpSocket;
    asio::ip::udp::endpoint m_clientUdpEndpoint;
    asio::ip::udp::endpoint m_boundEndpoint;
    asio::ip::udp::endpoint m_targetEndpoint;

    std::array<uint8_t, 65536> m_clientUdpBuffer;
    std::array<uint8_t, 65536> m_targetUdpBuffer;

    std::atomic<bool> m_active{false};
};