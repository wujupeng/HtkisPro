#pragma once

#include <cstdint>
#include <vector>
#include <string>

enum class ProtocolType {
    HTTP_CONNECT,
    HTTPS,
    SOCKS4,
    SOCKS4A,
    SOCKS5_TCP,
    SOCKS5_UDP,
    WEBSOCKET,
    SHADOWSOCKS,
    VMESS,
    VLESS,
    TROJAN,
    UNKNOWN
};

enum class SessionEndReason {
    ClientDisconnect,
    RemoteDisconnect,
    Timeout,
    AuthFailed,
    ConnectFailed,
    AclDenied,
    RateLimitExceeded,
    RelayDisconnect,
    Error,
    Unknown
};

class ProtocolDetector {
public:
    ProtocolDetector();

    void feed(const uint8_t* data, std::size_t length);
    ProtocolType detect() const;
    bool needsMoreData() const;
    bool exceededLimit() const;

    static const std::size_t MAX_BUFFER = 4096;

private:
    std::vector<uint8_t> m_buffer;
    ProtocolType m_result{ProtocolType::UNKNOWN};
    bool m_detected{false};

    void tryDetect();
    static bool isHttpMethod(const uint8_t* data, std::size_t length);
    static bool isWebSocketUpgrade(const uint8_t* data, std::size_t length);
};

const char* protocolTypeToString(ProtocolType type);