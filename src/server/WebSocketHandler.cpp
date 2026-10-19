#include "WebSocketHandler.h"
#include "WebSocketFrame.h"
#include <cstring>

bool WebSocketHandler::isUpgradeRequest(const uint8_t* data, std::size_t length) {
    std::string_view sv(reinterpret_cast<const char*>(data), length);
    return sv.find("Upgrade: websocket") != std::string_view::npos ||
           sv.find("upgrade: websocket") != std::string_view::npos;
}

std::string WebSocketHandler::extractWebSocketKey(const uint8_t* data, std::size_t length) {
    std::string_view sv(reinterpret_cast<const char*>(data), length);
    auto pos = sv.find("Sec-WebSocket-Key:");
    if (pos == std::string_view::npos) {
        pos = sv.find("sec-websocket-key:");
        if (pos == std::string_view::npos) return {};
    }

    auto valueStart = pos + 18;
    while (valueStart < sv.size() && sv[valueStart] == ' ') valueStart++;
    auto valueEnd = sv.find("\r\n", valueStart);
    if (valueEnd == std::string_view::npos) valueEnd = sv.size();

    return std::string(sv.substr(valueStart, valueEnd - valueStart));
}

std::string WebSocketHandler::buildUpgradeResponse(const std::string& key) {
    std::string acceptKey = WebSocketFrameCoder::computeAcceptKey(key);
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
           "\r\n";
}