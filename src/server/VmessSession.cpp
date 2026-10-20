#include "VmessSession.h"
#include "VmessHeaderDecoder.h"
#include "AesCfbCipher.h"
#include "AeadCipher.h"
#include "Logger.h"
#include "Authenticator.h"
#include <openssl/evp.h>
#include <cstring>

using asio::ip::tcp;

VmessSession::VmessSession(tcp::socket socket,
                           const std::vector<std::string>& uuids,
                           uint8_t alterId)
    : CryptoSessionBase(std::move(socket)),
      m_uuids(uuids),
      m_alterId(alterId) {
    m_protocol_name = "VMESS";
    m_protocol = ProtocolType::VMESS;
}

void VmessSession::onHandshake() {
    doReadHeader();
}

void VmessSession::doReadHeader() {
    auto self = std::static_pointer_cast<VmessSession>(shared_from_this());

    m_recv_buf.resize(256);
    m_client_socket.async_read_some(asio::buffer(m_recv_buf),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                for (const auto& uuid : m_uuids) {
                    auto [key, iv] = AesCfbCipher::deriveVmessKey(uuid);

                    auto decrypted = AesCfbCipher::decrypt(m_recv_buf.data(), length,
                                                           key.data(), key.size(),
                                                           iv.data(), iv.size());
                    auto header = VmessHeaderDecoder::decode(decrypted.data(), decrypted.size());
                    if (!header) continue;

                    std::string uuidStr(header->uuid.begin(), header->uuid.end());
                    if (!Authenticator::getInstance().authenticateUuid(uuid, m_client_addr, "vmess")) {
                        continue;
                    }

                    if (header->command != 0x01) {
                        Logger::getInstance().warn("[VMESS] Unsupported command " + std::to_string(header->command) + " from " + m_client_addr);
                        closeSession(SessionEndReason::Error);
                        return;
                    }

                    m_target_host = header->host;
                    m_target_port = header->port;

                    std::vector<uint8_t> sessionKey(32, 0);
                    EVP_RAND *rand = EVP_RAND_fetch(nullptr, "CTR-DRBG", nullptr);
                    if (rand) {
                        EVP_RAND_CTX *ctx = EVP_RAND_CTX_new(rand, nullptr);
                        if (ctx) {
                            EVP_RAND_generate(ctx, sessionKey.data(), 32, 0, 0, nullptr, 0);
                            EVP_RAND_CTX_free(ctx);
                        }
                        EVP_RAND_free(rand);
                    }
                    m_sessionKey = sessionKey;
                    m_cipher = AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, m_sessionKey.data());
                    m_chunk_codec = std::make_unique<VmessChunkCodec>(AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, m_sessionKey.data()));

                    Logger::getInstance().info("[VMESS] CONNECT " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr);
                    doResolve(m_target_host, m_target_port);
                    return;
                }

                Logger::getInstance().warn("[VMESS] Header decode/UUID validation failed from " + m_client_addr);
                closeSession(SessionEndReason::AuthFailed);
            } else {
                closeSession(SessionEndReason::ClientDisconnect);
            }
        });
}

std::vector<uint8_t> VmessSession::encryptToClient(const uint8_t* data, std::size_t length) {
    if (m_chunk_codec) {
        return m_chunk_codec->encode(data, length);
    }
    return std::vector<uint8_t>(data, data + length);
}

std::optional<std::vector<uint8_t>> VmessSession::decryptFromClient(const uint8_t* data, std::size_t length) {
    if (m_chunk_codec) {
        return m_chunk_codec->decode(data, length);
    }
    return std::vector<uint8_t>(data, data + length);
}