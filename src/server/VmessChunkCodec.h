#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <memory>
#include "AeadCipher.h"

class VmessChunkCodec {
public:
    explicit VmessChunkCodec(std::unique_ptr<AeadCipher> cipher);

    std::vector<uint8_t> encode(const uint8_t* data, std::size_t length);
    std::optional<std::vector<uint8_t>> decode(const uint8_t* data, std::size_t length);

    uint64_t sendCounter() const { return m_send_counter; }
    uint64_t recvCounter() const { return m_recv_counter; }

private:
    std::unique_ptr<AeadCipher> m_cipher;
    uint64_t m_send_counter{0};
    uint64_t m_recv_counter{0};

    static constexpr std::size_t CHUNK_HEADER = 2;
    static constexpr std::size_t TAG_LEN = 16;
    static constexpr std::size_t MAX_PAYLOAD = 0x3FFF;
};