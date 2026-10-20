#include "StunClient.h"
#include "Logger.h"
#include <random>
#include <cstring>
#include <chrono>

using asio::ip::udp;

StunClient::StunClient(asio::io_context& io)
    : m_io(io) {
}

std::vector<uint8_t> StunClient::buildBindingRequest() {
    std::vector<uint8_t> request(20);
    request[0] = 0x00; request[1] = 0x01;
    request[2] = 0x00; request[3] = 0x00;
    request[4] = 0x21; request[5] = 0x12; request[6] = 0xA4; request[7] = 0x42;

    std::random_device rd;
    std::mt19937 gen(rd());
    for (int i = 8; i < 20; ++i) {
        request[i] = static_cast<uint8_t>(gen() & 0xFF);
    }
    return request;
}

std::optional<NatInfo> StunClient::parseBindingResponse(const std::vector<uint8_t>& response,
                                                          const std::array<uint8_t, 12>& transactionId) {
    if (response.size() < 20) return std::nullopt;

    uint16_t msgType = (static_cast<uint16_t>(response[0]) << 8) | response[1];
    if (msgType != STUN_BINDING_RESPONSE) return std::nullopt;

    for (int i = 0; i < 12; ++i) {
        if (response[8 + i] != transactionId[i]) return std::nullopt;
    }

    uint16_t msgLen = (static_cast<uint16_t>(response[2]) << 8) | response[3];
    std::size_t offset = 20;

    NatInfo info;
    bool found = false;

    while (offset + 4 <= 20 + msgLen && offset + 4 <= response.size()) {
        uint16_t attrType = (static_cast<uint16_t>(response[offset]) << 8) | response[offset + 1];
        uint16_t attrLen = (static_cast<uint16_t>(response[offset + 2]) << 8) | response[offset + 3];
        offset += 4;

        if (offset + attrLen > response.size()) break;

        if (attrType == ATTR_XOR_MAPPED_ADDRESS && attrLen >= 8) {
            uint8_t family = response[offset + 1];
            if (family == 0x01 && attrLen >= 8) {
                uint16_t xport = (static_cast<uint16_t>(response[offset + 2]) << 8) | response[offset + 3];
                info.publicPort = xport ^ (static_cast<uint16_t>(STUN_MAGIC_COOKIE >> 16));

                uint32_t xip = (static_cast<uint32_t>(response[offset + 4]) << 24) |
                               (static_cast<uint32_t>(response[offset + 5]) << 16) |
                               (static_cast<uint32_t>(response[offset + 6]) << 8) |
                               response[offset + 7];
                uint32_t ip = xip ^ STUN_MAGIC_COOKIE;

                char buf[16];
                snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                         (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
                info.publicIp = buf;
                found = true;
            }
        } else if (attrType == ATTR_MAPPED_ADDRESS && attrLen >= 8 && !found) {
            uint8_t family = response[offset + 1];
            if (family == 0x01 && attrLen >= 8) {
                info.publicPort = (static_cast<uint16_t>(response[offset + 2]) << 8) | response[offset + 3];
                char buf[16];
                snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                         response[offset + 4], response[offset + 5],
                         response[offset + 6], response[offset + 7]);
                info.publicIp = buf;
                found = true;
            }
        }

        offset += attrLen;
        if (attrLen % 4 != 0) {
            offset += 4 - (attrLen % 4);
        }
    }

    if (found) return info;
    return std::nullopt;
}

std::optional<NatInfo> StunClient::probe(const std::string& stunServer, uint16_t stunPort,
                                          uint16_t localPort, uint32_t timeoutMs) {
    try {
        udp::socket socket(m_io, udp::endpoint(udp::v4(), localPort));
        udp::resolver resolver(m_io);
        auto endpoints = resolver.resolve(stunServer, std::to_string(stunPort));

        auto request = buildBindingRequest();
        std::array<uint8_t, 12> transactionId;
        std::memcpy(transactionId.data(), &request[8], 12);

        socket.send_to(asio::buffer(request), *endpoints.begin());

        std::vector<uint8_t> response(2048);
        socket.async_receive(asio::buffer(response),
            [](std::error_code, std::size_t) {});

        asio::steady_timer timer(m_io, std::chrono::milliseconds(timeoutMs));
        timer.wait();

        return parseBindingResponse(response, transactionId);
    } catch (const std::exception& e) {
        Logger::getInstance().warn("[STUN] Probe failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

Config::NatType StunClient::detectNatType(asio::io_context& io,
                                           const std::string& stunServer, uint16_t stunPort,
                                           uint32_t timeoutMs) {
    StunClient client1(io);
    auto info1 = client1.probe(stunServer, stunPort, 0, timeoutMs);
    if (!info1) return Config::NatType::UNKNOWN;

    StunClient client2(io);
    auto info2 = client2.probe(stunServer, stunPort, 0, timeoutMs);
    if (!info2) return Config::NatType::UNKNOWN;

    if (info1->publicIp == info2->publicIp && info1->publicPort == info2->publicPort) {
        return Config::NatType::FULL_CONE;
    }

    StunClient client3(io);
    auto info3 = client3.probe(stunServer, stunPort, 0, timeoutMs);
    if (!info3) return Config::NatType::RESTRICTED;

    if (info2->publicIp == info3->publicIp && info2->publicPort == info3->publicPort) {
        return Config::NatType::RESTRICTED;
    }

    return Config::NatType::SYMMETRIC;
}