#include "WebSocketFrame.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#endif

std::optional<WebSocketFrame> WebSocketFrameCoder::decode(const uint8_t* data, std::size_t length, std::size_t& consumed) {
    if (length < 2) return std::nullopt;

    WebSocketFrame frame;
    frame.fin = (data[0] & 0x80) != 0;
    frame.opcode = static_cast<WebSocketOpcode>(data[0] & 0x0F);
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payloadLen = data[1] & 0x7F;

    std::size_t headerLen = 2;
    if (payloadLen == 126) {
        if (length < 4) return std::nullopt;
        payloadLen = (static_cast<uint64_t>(data[2]) << 8) | data[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (length < 10) return std::nullopt;
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | data[2 + i];
        }
        headerLen = 10;
    }

    uint8_t maskKey[4] = {0};
    if (masked) {
        if (length < headerLen + 4) return std::nullopt;
        std::memcpy(maskKey, data + headerLen, 4);
        headerLen += 4;
    }

    if (length < headerLen + payloadLen) return std::nullopt;

    frame.payload.resize(static_cast<std::size_t>(payloadLen));
    if (masked) {
        for (std::size_t i = 0; i < payloadLen; i++) {
            frame.payload[i] = data[headerLen + i] ^ maskKey[i % 4];
        }
    } else {
        std::memcpy(frame.payload.data(), data + headerLen, static_cast<std::size_t>(payloadLen));
    }

    consumed = headerLen + static_cast<std::size_t>(payloadLen);
    return frame;
}

std::vector<uint8_t> WebSocketFrameCoder::encode(WebSocketOpcode opcode, const uint8_t* data, std::size_t length, bool masked) {
    std::vector<uint8_t> frame;
    frame.push_back(0x80 | static_cast<uint8_t>(opcode));

    if (length < 126) {
        frame.push_back(static_cast<uint8_t>((masked ? 0x80 : 0) | length));
    } else if (length < 65536) {
        frame.push_back(static_cast<uint8_t>((masked ? 0x80 : 0) | 126));
        frame.push_back(static_cast<uint8_t>(length >> 8));
        frame.push_back(static_cast<uint8_t>(length & 0xFF));
    } else {
        frame.push_back(static_cast<uint8_t>((masked ? 0x80 : 0) | 127));
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((length >> (i * 8)) & 0xFF));
        }
    }

    if (masked) {
        uint8_t maskKey[4] = {0x12, 0x34, 0x56, 0x78};
        for (auto b : maskKey) frame.push_back(b);
        for (std::size_t i = 0; i < length; i++) {
            frame.push_back(data[i] ^ maskKey[i % 4]);
        }
    } else {
        frame.insert(frame.end(), data, data + length);
    }

    return frame;
}

std::vector<uint8_t> WebSocketFrameCoder::encode(WebSocketOpcode opcode, const std::string& data, bool masked) {
    return encode(opcode, reinterpret_cast<const uint8_t*>(data.data()), data.size(), masked);
}

std::string WebSocketFrameCoder::computeAcceptKey(const std::string& key) {
    std::string combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashLen = 0;
    DWORD cbData = 0;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(DWORD), &cbData, 0);
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(combined.data())), static_cast<ULONG>(combined.size()), 0);

    std::vector<uint8_t> hash(hashLen);
    BCryptFinishHash(hHash, hash.data(), hashLen, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    DWORD encLen = 0;
    CryptBinaryToStringA(hash.data(), static_cast<DWORD>(hash.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encLen);
    std::string result(encLen, 0);
    CryptBinaryToStringA(hash.data(), static_cast<DWORD>(hash.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &result[0], &encLen);
    result.resize(encLen);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) result.pop_back();
    return result;
#else
    return {};
#endif
}