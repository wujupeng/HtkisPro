#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

struct VlessHeader {
    uint8_t version{0};
    std::vector<uint8_t> uuid;
    std::vector<uint8_t> addons;
    uint8_t command{0};
    uint16_t port{0};
    uint8_t addrType{0};
    std::string host;
};

class VlessHeaderParser {
public:
    static std::optional<VlessHeader> parse(const uint8_t* data, std::size_t length);
    static std::vector<uint8_t> buildResponse(uint8_t version);
};