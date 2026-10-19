#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

enum class WebSocketOpcode : uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
};

struct WebSocketFrame {
    bool fin = false;
    WebSocketOpcode opcode = WebSocketOpcode::Binary;
    std::vector<uint8_t> payload;
};

class WebSocketFrameCoder {
public:
    static std::optional<WebSocketFrame> decode(const uint8_t* data, std::size_t length, std::size_t& consumed);
    static std::vector<uint8_t> encode(WebSocketOpcode opcode, const uint8_t* data, std::size_t length, bool masked = false);
    static std::vector<uint8_t> encode(WebSocketOpcode opcode, const std::string& data, bool masked = false);
    static std::string computeAcceptKey(const std::string& key);
};