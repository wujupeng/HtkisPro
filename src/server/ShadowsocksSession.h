#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "CryptoSessionBase.h"
#include "Config.h"

class ShadowsocksSession : public CryptoSessionBase {
public:
    ShadowsocksSession(asio::ip::tcp::socket socket,
                       const std::string& password,
                       Config::SsMethod method);

protected:
    void onHandshake() override;
    std::vector<uint8_t> encryptToClient(const uint8_t* data, std::size_t length) override;
    std::optional<std::vector<uint8_t>> decryptFromClient(const uint8_t* data, std::size_t length) override;

private:
    void doReadSalt();
    void doReadEncryptedHeader();
    void doReadEncryptedPayload(uint16_t payloadLen);
    void parseSocksAddress(const uint8_t* data, std::size_t length);

    static std::vector<uint8_t> hkdfSha1(const uint8_t* masterKey, std::size_t keyLen,
                                          const uint8_t* salt, std::size_t saltLen,
                                          std::size_t outLen);

    std::string m_password;
    Config::SsMethod m_method;
    std::vector<uint8_t> m_masterKey;
    std::size_t m_saltLength{0};

    std::vector<uint8_t> m_recv_buffer;
    std::vector<uint8_t> m_decrypted_accum;

    bool m_address_parsed{false};
    uint64_t m_recv_chunk_counter{0};
    uint64_t m_send_chunk_counter{0};

    static constexpr std::size_t SS_MAX_PAYLOAD = 0x3FFF;
    static constexpr std::size_t SS_HEADER_LEN = 2;
    static constexpr std::size_t SS_TAG_LEN = 16;
};