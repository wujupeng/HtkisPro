#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <ctime>
#include <cstdint>
#include <optional>
#include "ProtocolDetector.h"
#include "AeadCipher.h"

class CryptoSessionBase : public std::enable_shared_from_this<CryptoSessionBase> {
public:
    explicit CryptoSessionBase(asio::ip::tcp::socket socket);
    virtual ~CryptoSessionBase();

    void start();

protected:
    virtual void onHandshake() = 0;

    virtual std::vector<uint8_t> encryptToClient(const uint8_t* data, std::size_t length);
    virtual std::optional<std::vector<uint8_t>> decryptFromClient(const uint8_t* data, std::size_t length);

    void doResolve(const std::string& host, uint16_t port);
    void doConnect(const asio::ip::tcp::resolver::results_type& endpoints);
    void doStream();
    void doReadClient();
    void doWriteRemote(std::size_t length);
    void doReadRemote();
    void doWriteClient(std::size_t length);
    void closeSession(SessionEndReason reason);

    static std::vector<uint8_t> generateNonce(uint64_t counter, std::size_t nonceLength);
    void zeroSessionKey();

    asio::ip::tcp::socket m_client_socket;
    asio::ip::tcp::socket m_remote_socket;
    asio::ip::tcp::resolver m_resolver;

    std::array<uint8_t, 65536> m_client_buffer;
    std::array<uint8_t, 65536> m_remote_buffer;

    std::unique_ptr<AeadCipher> m_cipher;
    std::vector<uint8_t> m_sessionKey;
    uint64_t m_sendNonceCounter{0};
    uint64_t m_recvNonceCounter{0};

    std::string m_client_addr;
    std::string m_target_host;
    uint16_t m_target_port{0};
    ProtocolType m_protocol{ProtocolType::UNKNOWN};
    std::string m_protocol_name;
    std::atomic<uint64_t> m_bytes_up{0};
    std::atomic<uint64_t> m_bytes_down{0};
    std::time_t m_start_time{0};
    bool m_tunnel_active{false};

    asio::steady_timer m_idle_timer;
    uint32_t m_idle_timeout_seconds{300};

private:
    void resetIdleTimer();
    void onIdleTimeout(std::error_code ec);
};