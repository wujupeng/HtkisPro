#include "ShadowsocksSession.h"
#include "Logger.h"
#include "Authenticator.h"
#include "AeadCipher.h"
#include "SsKeyDerivation.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <cstring>

using asio::ip::tcp;

ShadowsocksSession::ShadowsocksSession(tcp::socket socket,
                                       const std::string& password,
                                       Config::SsMethod method)
    : CryptoSessionBase(std::move(socket)),
      m_password(password),
      m_method(method) {
    m_protocol_name = "SS";
    m_protocol = ProtocolType::SHADOWSOCKS;

    switch (method) {
        case Config::SsMethod::AES_128_GCM:
            m_saltLength = 16;
            break;
        case Config::SsMethod::AES_256_GCM:
        case Config::SsMethod::CHACHA20_IETF_POLY1305:
            m_saltLength = 32;
            break;
    }
}

std::vector<uint8_t> ShadowsocksSession::hkdfSha1(const uint8_t* masterKey, std::size_t keyLen,
                                                   const uint8_t* salt, std::size_t saltLen,
                                                   std::size_t outLen) {
    unsigned char prk[SHA_DIGEST_LENGTH];
    unsigned int prkLen = 0;
    HMAC(EVP_sha1(), masterKey, static_cast<int>(keyLen), salt, saltLen, prk, &prkLen);

    std::vector<uint8_t> okm;
    std::vector<uint8_t> t;
    const std::string info = "ss-subkey";
    unsigned char tk[SHA_DIGEST_LENGTH];
    unsigned int tkLen = 0;

    for (uint8_t i = 1; okm.size() < outLen; ++i) {
        std::vector<uint8_t> hmacInput;
        hmacInput.insert(hmacInput.end(), t.begin(), t.end());
        hmacInput.insert(hmacInput.end(), info.begin(), info.end());
        hmacInput.push_back(i);

        HMAC(EVP_sha1(), prk, prkLen, hmacInput.data(), hmacInput.size(), tk, &tkLen);
        t.assign(tk, tk + tkLen);
        okm.insert(okm.end(), t.begin(), t.end());
    }
    okm.resize(outLen);
    return okm;
}

void ShadowsocksSession::onHandshake() {
    doReadSalt();
}

void ShadowsocksSession::doReadSalt() {
    auto self = std::static_pointer_cast<ShadowsocksSession>(shared_from_this());
    m_recv_buffer.resize(m_saltLength);
    asio::async_read(m_client_socket, asio::buffer(m_recv_buffer),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                std::size_t keyLen = 0;
                switch (m_method) {
                    case Config::SsMethod::AES_128_GCM:  keyLen = 16; break;
                    case Config::SsMethod::AES_256_GCM:  keyLen = 32; break;
                    case Config::SsMethod::CHACHA20_IETF_POLY1305: keyLen = 32; break;
                }
                m_masterKey = SsKeyDerivation::derive(m_password, keyLen);
                m_sessionKey = hkdfSha1(m_masterKey.data(), m_masterKey.size(),
                                        m_recv_buffer.data(), m_recv_buffer.size(), keyLen);
                m_cipher = AeadCipherFactory::create(m_method, m_sessionKey.data());
                Logger::getInstance().info("[SS] Session key derived, salt=" + std::to_string(m_recv_buffer.size()) + " bytes");
                doReadEncryptedHeader();
            } else {
                Logger::getInstance().warn("[SS] Salt read error from " + m_client_addr + ": " + ec.message());
                closeSession(SessionEndReason::Error);
            }
        });
}

void ShadowsocksSession::doReadEncryptedHeader() {
    auto self = std::static_pointer_cast<ShadowsocksSession>(shared_from_this());
    std::size_t headerSize = SS_HEADER_LEN + SS_TAG_LEN;
    m_recv_buffer.resize(headerSize);

    asio::async_read(m_client_socket, asio::buffer(m_recv_buffer),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                auto nonce = generateNonce(m_recv_chunk_counter, m_cipher->nonceLength());
                auto decrypted = m_cipher->decrypt(m_recv_buffer.data(), m_recv_buffer.size(),
                                                   nullptr, 0, nonce.data());
                if (!decrypted || decrypted->size() < 2) {
                    Logger::getInstance().warn("[SS] AEAD header decrypt failed from " + m_client_addr);
                    Authenticator::getInstance().recordShadowsocksFailure(m_client_addr);
                    closeSession(SessionEndReason::AuthFailed);
                    return;
                }
                m_recv_chunk_counter += 1;

                uint16_t payloadLen = (static_cast<uint16_t>((*decrypted)[0]) << 8) | (*decrypted)[1];
                if (payloadLen > SS_MAX_PAYLOAD) {
                    Logger::getInstance().warn("[SS] Payload too large: " + std::to_string(payloadLen) + " from " + m_client_addr);
                    closeSession(SessionEndReason::Error);
                    return;
                }
                doReadEncryptedPayload(payloadLen);
            } else {
                closeSession(SessionEndReason::ClientDisconnect);
            }
        });
}

void ShadowsocksSession::doReadEncryptedPayload(uint16_t payloadLen) {
    auto self = std::static_pointer_cast<ShadowsocksSession>(shared_from_this());
    std::size_t chunkSize = payloadLen + SS_TAG_LEN;
    m_recv_buffer.resize(chunkSize);

    asio::async_read(m_client_socket, asio::buffer(m_recv_buffer),
        [this, self, payloadLen](std::error_code ec, std::size_t) {
            if (!ec) {
                auto nonce = generateNonce(m_recv_chunk_counter, m_cipher->nonceLength());
                auto decrypted = m_cipher->decrypt(m_recv_buffer.data(), m_recv_buffer.size(),
                                                   nullptr, 0, nonce.data());
                if (!decrypted) {
                    Logger::getInstance().warn("[SS] AEAD payload decrypt failed from " + m_client_addr);
                    closeSession(SessionEndReason::Error);
                    return;
                }
                m_recv_chunk_counter += 1;

                if (!m_address_parsed) {
                    parseSocksAddress(decrypted->data(), decrypted->size());
                } else {
                    m_decrypted_accum = std::move(*decrypted);
                    doWriteRemote(m_decrypted_accum.size());
                    std::memcpy(m_client_buffer.data(), m_decrypted_accum.data(), m_decrypted_accum.size());
                }
            } else {
                closeSession(SessionEndReason::ClientDisconnect);
            }
        });
}

void ShadowsocksSession::parseSocksAddress(const uint8_t* data, std::size_t length) {
    if (length < 1) {
        closeSession(SessionEndReason::Error);
        return;
    }
    uint8_t atyp = data[0];
    std::size_t addrStart = 1;
    std::string host;

    if (atyp == 0x01) {
        if (length < 1 + 4 + 2) { closeSession(SessionEndReason::Error); return; }
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", data[1], data[2], data[3], data[4]);
        host = buf;
        addrStart = 1 + 4;
    } else if (atyp == 0x03) {
        if (length < 1 + 1) { closeSession(SessionEndReason::Error); return; }
        uint8_t dlen = data[1];
        if (length < 1 + 1 + dlen + 2) { closeSession(SessionEndReason::Error); return; }
        host = std::string(reinterpret_cast<const char*>(&data[2]), dlen);
        addrStart = 1 + 1 + dlen;
    } else if (atyp == 0x04) {
        if (length < 1 + 16 + 2) { closeSession(SessionEndReason::Error); return; }
        char buf[64];
        snprintf(buf, sizeof(buf), "%x:%x:%x:%x:%x:%x:%x:%x",
                 (data[1]<<8)|data[2], (data[3]<<8)|data[4], (data[5]<<8)|data[6], (data[7]<<8)|data[8],
                 (data[9]<<8)|data[10], (data[11]<<8)|data[12], (data[13]<<8)|data[14], (data[15]<<8)|data[16]);
        host = buf;
        addrStart = 1 + 16;
    } else {
        closeSession(SessionEndReason::Error);
        return;
    }

    if (length < addrStart + 2) { closeSession(SessionEndReason::Error); return; }
    m_target_host = host;
    m_target_port = (static_cast<uint16_t>(data[addrStart]) << 8) | data[addrStart + 1];
    m_address_parsed = true;

    Logger::getInstance().info("[SS] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);

    std::size_t remaining = length - (addrStart + 2);
    if (remaining > 0) {
        m_decrypted_accum.assign(data + addrStart + 2, data + length);
    }
    doResolve(m_target_host, m_target_port);
}

std::vector<uint8_t> ShadowsocksSession::encryptToClient(const uint8_t* data, std::size_t length) {
    std::vector<uint8_t> output;
    output.reserve(length + SS_TAG_LEN + SS_HEADER_LEN + SS_TAG_LEN);

    std::size_t offset = 0;
    while (offset < length) {
        std::size_t chunkLen = (std::min)(length - offset, SS_MAX_PAYLOAD);
        uint16_t chunkLen16 = static_cast<uint16_t>(chunkLen);

        uint8_t header[2] = {static_cast<uint8_t>(chunkLen16 >> 8), static_cast<uint8_t>(chunkLen16 & 0xFF)};
        auto nonce1 = generateNonce(m_send_chunk_counter, m_cipher->nonceLength());
        auto encHeader = m_cipher->encrypt(header, 2, nullptr, 0, nonce1.data());
        output.insert(output.end(), encHeader.begin(), encHeader.end());
        m_send_chunk_counter += 1;

        auto nonce2 = generateNonce(m_send_chunk_counter, m_cipher->nonceLength());
        auto encPayload = m_cipher->encrypt(data + offset, chunkLen, nullptr, 0, nonce2.data());
        output.insert(output.end(), encPayload.begin(), encPayload.end());
        m_send_chunk_counter += 1;

        offset += chunkLen;
    }
    return output;
}

std::optional<std::vector<uint8_t>> ShadowsocksSession::decryptFromClient(const uint8_t* data, std::size_t length) {
    return std::vector<uint8_t>(data, data + length);
}