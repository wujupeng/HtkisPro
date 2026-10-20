#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include "CryptoSessionBase.h"
#include "VmessChunkCodec.h"
#include "Config.h"

class VmessSession : public CryptoSessionBase {
public:
    VmessSession(asio::ip::tcp::socket socket,
                 const std::vector<std::string>& uuids,
                 uint8_t alterId);

protected:
    void onHandshake() override;
    std::vector<uint8_t> encryptToClient(const uint8_t* data, std::size_t length) override;
    std::optional<std::vector<uint8_t>> decryptFromClient(const uint8_t* data, std::size_t length) override;

private:
    void doReadHeader();
    void processHeader(const std::vector<uint8_t>& decrypted);

    std::vector<std::string> m_uuids;
    uint8_t m_alterId;
    std::unique_ptr<VmessChunkCodec> m_chunk_codec;
    std::vector<uint8_t> m_recv_buf;
};