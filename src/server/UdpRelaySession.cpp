#include "UdpRelaySession.h"
#include "Logger.h"

UdpRelaySession::UdpRelaySession(asio::io_context& io, asio::ip::tcp::socket& controlSocket)
    : m_io(io),
      m_controlSocket(controlSocket),
      m_udpSocket(io),
      m_targetUdpSocket(io) {
}

UdpRelaySession::~UdpRelaySession() {
    stop();
}

void UdpRelaySession::start() {
    m_udpSocket.open(asio::ip::udp::v4());
    m_udpSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
    m_boundEndpoint = m_udpSocket.local_endpoint();
    m_targetUdpSocket.open(asio::ip::udp::v4());
    m_active = true;

    Logger::getInstance().info("[UDP] UDP relay started on " +
        m_boundEndpoint.address().to_string() + ":" + std::to_string(m_boundEndpoint.port()));
    doReceive();
}

void UdpRelaySession::stop() {
    if (!m_active.exchange(false)) return;
    std::error_code ec;
    m_udpSocket.close(ec);
    m_targetUdpSocket.close(ec);
    Logger::getInstance().info("[UDP] UDP relay stopped");
}

void UdpRelaySession::doReceive() {
    if (!m_active) return;
    auto self(shared_from_this());
    m_udpSocket.async_receive_from(
        asio::buffer(m_clientUdpBuffer), m_clientUdpEndpoint,
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec && m_active) {
                handleClientUdp(length, m_clientUdpEndpoint);
            } else if (ec) {
                Logger::getInstance().warn("[UDP] Receive error: " + ec.message());
            }
        });
}

void UdpRelaySession::handleClientUdp(std::size_t length, asio::ip::udp::endpoint sender) {
    if (length < 10) { doReceive(); return; }

    uint16_t rsv = (m_clientUdpBuffer[0] << 8) | m_clientUdpBuffer[1];
    if (rsv != 0) {
        Logger::getInstance().warn("[UDP] Invalid RSV field");
        doReceive();
        return;
    }

    uint8_t frag = m_clientUdpBuffer[2];
    if (frag != 0) {
        Logger::getInstance().warn("[UDP] Fragmentation not supported, dropping");
        doReceive();
        return;
    }

    uint8_t atyp = m_clientUdpBuffer[3];
    std::string targetHost;
    uint16_t targetPort;
    size_t dataOffset;

    if (atyp == 0x01) {
        if (length < 10) { doReceive(); return; }
        char ipBuf[16];
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u",
            m_clientUdpBuffer[4], m_clientUdpBuffer[5],
            m_clientUdpBuffer[6], m_clientUdpBuffer[7]);
        targetHost = ipBuf;
        targetPort = (m_clientUdpBuffer[8] << 8) | m_clientUdpBuffer[9];
        dataOffset = 10;
    } else if (atyp == 0x03) {
        if (length < 7) { doReceive(); return; }
        uint8_t domainLen = m_clientUdpBuffer[4];
        if (length < 5u + domainLen + 2) { doReceive(); return; }
        targetHost = std::string(reinterpret_cast<char*>(&m_clientUdpBuffer[5]), domainLen);
        targetPort = (m_clientUdpBuffer[5 + domainLen] << 8) | m_clientUdpBuffer[5 + domainLen + 1];
        dataOffset = 5 + domainLen + 2;
    } else {
        Logger::getInstance().warn("[UDP] Unsupported ATYP " + std::to_string(atyp));
        doReceive();
        return;
    }

    auto self(shared_from_this());
    m_targetEndpoint = asio::ip::udp::endpoint(
        asio::ip::make_address(targetHost), targetPort);

    if (dataOffset >= length) { doReceive(); return; }

    m_targetUdpSocket.async_send_to(
        asio::buffer(m_clientUdpBuffer.data() + dataOffset, length - dataOffset),
        m_targetEndpoint,
        [this, self, targetHost, targetPort](std::error_code ec, std::size_t) {
            if (!ec) {
                doReceiveFromTarget();
            } else {
                Logger::getInstance().warn("[UDP] Send to target failed: " + ec.message());
                doReceive();
            }
        });
}

void UdpRelaySession::doReceiveFromTarget() {
    if (!m_active) return;
    auto self(shared_from_this());
    m_targetUdpSocket.async_receive_from(
        asio::buffer(m_targetUdpBuffer), m_targetEndpoint,
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec && m_active) {
                handleTargetUdp(length);
            }
            doReceive();
        });
}

void UdpRelaySession::handleTargetUdp(std::size_t length) {
    std::vector<uint8_t> response;
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x01);

    auto addr = m_targetEndpoint.address();
    if (addr.is_v4()) {
        auto bytes = addr.to_v4().to_bytes();
        for (auto b : bytes) response.push_back(b);
    } else {
        response[3] = 0x04;
        auto bytes = addr.to_v6().to_bytes();
        for (auto b : bytes) response.push_back(b);
    }

    uint16_t port = m_targetEndpoint.port();
    response.push_back(static_cast<uint8_t>(port >> 8));
    response.push_back(static_cast<uint8_t>(port & 0xFF));
    response.insert(response.end(), m_targetUdpBuffer.begin(), m_targetUdpBuffer.begin() + length);

    auto self(shared_from_this());
    auto buf = std::make_shared<std::vector<uint8_t>>(std::move(response));
    m_udpSocket.async_send_to(
        asio::buffer(*buf), m_clientUdpEndpoint,
        [this, self, buf](std::error_code ec, std::size_t) {
            if (ec) {
                Logger::getInstance().warn("[UDP] Send to client failed: " + ec.message());
            }
        });
}