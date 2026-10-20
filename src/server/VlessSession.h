#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <vector>
#include <string>
#include "CryptoSessionBase.h"

class VlessSession : public CryptoSessionBase {
public:
    VlessSession(asio::ip::tcp::socket socket, const std::vector<std::string>& uuids);

protected:
    void onHandshake() override;

private:
    void doReadHeader();

    std::vector<std::string> m_uuids;
    std::vector<uint8_t> m_recv_buf;
};