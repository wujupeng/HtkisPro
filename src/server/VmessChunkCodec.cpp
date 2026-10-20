#include "VmessChunkCodec.h"
#include <cstring>

VmessChunkCodec::VmessChunkCodec(std::unique_ptr<AeadCipher> cipher)
    : m_cipher(std::move(cipher)) {
}

static std::vector<uint8_t> makeNonce(uint64_t counter, std::size_t len) {
    std::vector<uint8_t> nonce(len, 0);
    for (std::size_t i = 0; i < 8 && i < len; ++i) {
        nonce[i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
    }
    return nonce;
}

std::vector<uint8_t> VmessChunkCodec::encode(const uint8_t* data, std::size_t length) {
    std::vector<uint8_t> output;
    output.reserve(length + 64);

    std::size_t offset = 0;
    while (offset < length) {
        std::size_t chunkLen = (std::min)(length - offset, MAX_PAYLOAD);
        uint16_t chunkLen16 = static_cast<uint16_t>(chunkLen);

        uint8_t hdr[2] = {static_cast<uint8_t>(chunkLen16 >> 8), static_cast<uint8_t>(chunkLen16 & 0xFF)};
        auto nonce1 = makeNonce(m_send_counter, m_cipher->nonceLength());
        auto encHdr = m_cipher->encrypt(hdr, 2, nullptr, 0, nonce1.data());
        output.insert(output.end(), encHdr.begin(), encHdr.end());
        m_send_counter += 1;

        auto nonce2 = makeNonce(m_send_counter, m_cipher->nonceLength());
        auto encPayload = m_cipher->encrypt(data + offset, chunkLen, nullptr, 0, nonce2.data());
        output.insert(output.end(), encPayload.begin(), encPayload.end());
        m_send_counter += 1;

        offset += chunkLen;
    }
    return output;
}

std::optional<std::vector<uint8_t>> VmessChunkCodec::decode(const uint8_t* data, std::size_t length) {
    std::vector<uint8_t> output;
    std::size_t offset = 0;

    while (offset < length) {
        if (offset + CHUNK_HEADER + TAG_LEN > length) break;

        auto nonce1 = makeNonce(m_recv_counter, m_cipher->nonceLength());
        auto decHdr = m_cipher->decrypt(data + offset, CHUNK_HEADER + TAG_LEN, nullptr, 0, nonce1.data());
        if (!decHdr || decHdr->size() < 2) return std::nullopt;
        m_recv_counter += 1;

        uint16_t payloadLen = (static_cast<uint16_t>((*decHdr)[0]) << 8) | (*decHdr)[1];
        if (payloadLen > MAX_PAYLOAD) return std::nullopt;

        offset += CHUNK_HEADER + TAG_LEN;
        if (offset + payloadLen + TAG_LEN > length) break;

        auto nonce2 = makeNonce(m_recv_counter, m_cipher->nonceLength());
        auto decPayload = m_cipher->decrypt(data + offset, payloadLen + TAG_LEN, nullptr, 0, nonce2.data());
        if (!decPayload) return std::nullopt;
        m_recv_counter += 1;

        output.insert(output.end(), decPayload->begin(), decPayload->end());
        offset += payloadLen + TAG_LEN;
    }
    return output;
}