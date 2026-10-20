#include "VlessHeaderParser.h"

std::optional<VlessHeader> VlessHeaderParser::parse(const uint8_t* data, std::size_t length) {
    VlessHeader h;
    std::size_t offset = 0;

    if (length < 1 + 16 + 1) return std::nullopt;
    h.version = data[offset]; offset += 1;
    h.uuid.assign(data + offset, data + offset + 16); offset += 16;

    uint8_t addonsLen = data[offset]; offset += 1;
    if (offset + addonsLen > length) return std::nullopt;
    h.addons.assign(data + offset, data + offset + addonsLen); offset += addonsLen;

    if (offset + 1 + 2 + 1 > length) return std::nullopt;
    h.command = data[offset]; offset += 1;
    h.port = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]; offset += 2;
    h.addrType = data[offset]; offset += 1;

    if (h.addrType == 1) {
        if (offset + 4 > length) return std::nullopt;
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", data[offset], data[offset+1], data[offset+2], data[offset+3]);
        h.host = buf;
        offset += 4;
    } else if (h.addrType == 2) {
        if (offset + 1 > length) return std::nullopt;
        uint8_t dlen = data[offset]; offset += 1;
        if (offset + dlen > length) return std::nullopt;
        h.host = std::string(reinterpret_cast<const char*>(data + offset), dlen);
        offset += dlen;
    } else if (h.addrType == 3) {
        if (offset + 16 > length) return std::nullopt;
        char buf[64];
        snprintf(buf, sizeof(buf), "%x:%x:%x:%x:%x:%x:%x:%x",
                 (data[offset]<<8)|data[offset+1], (data[offset+2]<<8)|data[offset+3],
                 (data[offset+4]<<8)|data[offset+5], (data[offset+6]<<8)|data[offset+7],
                 (data[offset+8]<<8)|data[offset+9], (data[offset+10]<<8)|data[offset+11],
                 (data[offset+12]<<8)|data[offset+13], (data[offset+14]<<8)|data[offset+15]);
        h.host = buf;
        offset += 16;
    } else {
        return std::nullopt;
    }

    return h;
}

std::vector<uint8_t> VlessHeaderParser::buildResponse(uint8_t version) {
    return {version, 0x00};
}