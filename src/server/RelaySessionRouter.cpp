#include "RelaySessionRouter.h"
#include "RelayManager.h"
#include "Logger.h"

void RelaySessionRouter::onRelayRequest(const std::string& relayNodeId,
                                        const uint8_t* data, std::size_t length,
                                        ResponseCallback responseCallback) {
    Logger::getInstance().info("[RELAY-ROUTER] Request from node=" + relayNodeId + " length=" + std::to_string(length));

    if (length < 1) {
        Logger::getInstance().warn("[RELAY-ROUTER] Empty request");
        return;
    }

    responseCallback(data, length);
}

void RelaySessionRouter::routeByPriority(const uint8_t* data, std::size_t length,
                                         ResponseCallback responseCallback) {
    auto nodeId = RelayManager::getInstance().selectByPriority();
    if (!nodeId) {
        Logger::getInstance().warn("[RELAY-ROUTER] No active relay node available");
        return;
    }
    onRelayRequest(*nodeId, data, length, std::move(responseCallback));
}