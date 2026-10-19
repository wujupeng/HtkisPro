#pragma once

#include <string>
#include <string_view>

class WebSocketHandler {
public:
    static bool isUpgradeRequest(const uint8_t* data, std::size_t length);
    static std::string extractWebSocketKey(const uint8_t* data, std::size_t length);
    static std::string buildUpgradeResponse(const std::string& key);
};