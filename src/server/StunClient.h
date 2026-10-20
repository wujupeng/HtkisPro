#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <array>
#include "Config.h"

struct NatInfo {
    Config::NatType type{Config::NatType::UNKNOWN};
    std::string publicIp;
    uint16_t publicPort{0};
};

class StunClient {
public:
    explicit StunClient(asio::io_context& io);

    std::optional<NatInfo> probe(const std::string& stunServer, uint16_t stunPort,
                                  uint16_t localPort = 0, uint32_t timeoutMs = 5000);

    static Config::NatType detectNatType(asio::io_context& io,
                                          const std::string& stunServer, uint16_t stunPort,
                                          uint32_t timeoutMs = 5000);

private:
    static constexpr uint16_t STUN_BINDING_REQUEST = 0x0001;
    static constexpr uint16_t STUN_BINDING_RESPONSE = 0x0101;
    static constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;
    static constexpr uint16_t ATTR_MAPPED_ADDRESS = 0x0001;
    static constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;

    std::vector<uint8_t> buildBindingRequest();
    std::optional<NatInfo> parseBindingResponse(const std::vector<uint8_t>& response,
                                                  const std::array<uint8_t, 12>& transactionId);

    asio::io_context& m_io;
};