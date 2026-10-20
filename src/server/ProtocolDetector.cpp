#include "ProtocolDetector.h"
#include <cstring>

ProtocolDetector::ProtocolDetector() {
    m_buffer.reserve(MAX_BUFFER);
}

void ProtocolDetector::feed(const uint8_t* data, std::size_t length) {
    if (m_detected) return;
    m_buffer.insert(m_buffer.end(), data, data + length);
    tryDetect();
}

bool ProtocolDetector::needsMoreData() const {
    return !m_detected && m_buffer.size() < MAX_BUFFER;
}

bool ProtocolDetector::exceededLimit() const {
    return m_buffer.size() >= MAX_BUFFER && !m_detected;
}

void ProtocolDetector::tryDetect() {
    if (m_buffer.empty()) return;

    uint8_t firstByte = m_buffer[0];

    if (firstByte == 0x16) {
        m_result = ProtocolType::HTTPS;
        m_detected = true;
        return;
    }

    if (firstByte == 0x05) {
        if (m_buffer.size() >= 2) {
            m_result = ProtocolType::SOCKS5_TCP;
            m_detected = true;
        }
        return;
    }

    if (firstByte == 0x04) {
        m_result = ProtocolType::SOCKS4;
        m_detected = true;
        return;
    }

    if (isHttpMethod(m_buffer.data(), m_buffer.size())) {
        if (isWebSocketUpgrade(m_buffer.data(), m_buffer.size())) {
            m_result = ProtocolType::WEBSOCKET;
            m_detected = true;
        } else if (m_buffer.size() >= 8 && std::memcmp(m_buffer.data(), "CONNECT ", 8) == 0) {
            m_result = ProtocolType::HTTP_CONNECT;
            m_detected = true;
        } else {
            m_result = ProtocolType::HTTP_CONNECT;
            m_detected = true;
        }
        return;
    }
}

bool ProtocolDetector::isHttpMethod(const uint8_t* data, std::size_t length) {
    static const char* methods[] = {"CONNECT ", "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "OPTIONS "};
    for (const char* method : methods) {
        std::size_t len = std::strlen(method);
        if (length >= len && std::memcmp(data, method, len) == 0) return true;
    }
    return false;
}

bool ProtocolDetector::isWebSocketUpgrade(const uint8_t* data, std::size_t length) {
    const char* str = reinterpret_cast<const char*>(data);
    std::string_view sv(str, length);

    auto pos = sv.find("Upgrade:");
    if (pos == std::string_view::npos) {
        pos = sv.find("upgrade:");
        if (pos == std::string_view::npos) return false;
    }

    auto valueStart = pos + 8;
    while (valueStart < sv.size() && (sv[valueStart] == ' ' || sv[valueStart] == '\t')) valueStart++;
    auto valueEnd = sv.find("\r\n", valueStart);
    if (valueEnd == std::string_view::npos) valueEnd = sv.size();

    std::string_view value = sv.substr(valueStart, valueEnd - valueStart);
    return value == "websocket" || value == "WebSocket";
}

ProtocolType ProtocolDetector::detect() const {
    return m_result;
}

const char* protocolTypeToString(ProtocolType type) {
    switch (type) {
        case ProtocolType::HTTP_CONNECT: return "HTTP CONNECT";
        case ProtocolType::HTTPS: return "HTTPS";
        case ProtocolType::SOCKS4: return "SOCKS4";
        case ProtocolType::SOCKS4A: return "SOCKS4a";
        case ProtocolType::SOCKS5_TCP: return "SOCKS5 TCP";
        case ProtocolType::SOCKS5_UDP: return "SOCKS5 UDP";
        case ProtocolType::WEBSOCKET: return "WebSocket";
        case ProtocolType::SHADOWSOCKS: return "Shadowsocks";
        case ProtocolType::VMESS: return "VMess";
        case ProtocolType::VLESS: return "VLESS";
        case ProtocolType::TROJAN: return "Trojan";
        default: return "Unknown";
    }
}