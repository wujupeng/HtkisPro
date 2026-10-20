#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

struct VmessHeader {
    uint8_t version{0};
    std::vector<uint8_t> uuid;
    std::vector<uint8_t> authId;
    uint8_t command{0};
    uint16_t port{0};
    uint8_t addrType{0};
    std::string host;
    uint32_t checksum{0};
};

class VmessHeaderDecoder {
public:
    static std::optional<VmessHeader> decode(const uint8_t* data, std::size_t length);
    static bool validateChecksum(const uint8_t* data, std::size_t length, uint32_t checksum);
};