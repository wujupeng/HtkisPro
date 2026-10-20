#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <optional>
#include "StunClient.h"
#include "Config.h"

class NatTraversalManager {
public:
    static NatTraversalManager& getInstance() {
        static NatTraversalManager instance;
        return instance;
    }

    void enable(asio::io_context& io, std::shared_ptr<const Config::NatTraversalConfig> cfg);
    void disable();
    std::optional<NatInfo> probeNow();
    NatInfo getCurrentState() const;
    bool isEnabled() const;

private:
    NatTraversalManager() = default;

    void startKeepalive();
    void onKeepalive();

    mutable std::mutex m_mutex;
    asio::io_context* m_io{nullptr};
    std::unique_ptr<asio::steady_timer> m_timer;
    std::shared_ptr<const Config::NatTraversalConfig> m_config;
    NatInfo m_currentState;
    std::atomic<bool> m_enabled{false};
    std::unique_ptr<asio::io_context> m_probe_io;
    std::unique_ptr<std::thread> m_probe_thread;
};