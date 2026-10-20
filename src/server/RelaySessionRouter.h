#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

class RelaySessionRouter {
public:
    using ResponseCallback = std::function<void(const uint8_t*, std::size_t)>;

    static RelaySessionRouter& getInstance() {
        static RelaySessionRouter instance;
        return instance;
    }

    void onRelayRequest(const std::string& relayNodeId,
                        const uint8_t* data, std::size_t length,
                        ResponseCallback responseCallback);

    void routeByPriority(const uint8_t* data, std::size_t length,
                         ResponseCallback responseCallback);

private:
    RelaySessionRouter() = default;
};