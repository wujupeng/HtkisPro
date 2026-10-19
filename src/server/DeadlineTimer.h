#pragma once

#include "Config.h"
#include <asio.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>

class DeadlineTimer {
public:
    enum class Type {
        FirstPacket,
        Connect,
        Idle,
        UdpAssociate
    };

    explicit DeadlineTimer(asio::io_context& io);

    void start(Type type, std::function<void()> callback);
    void cancel(Type type);
    void cancelAll();
    void resetIdle();

    void refreshConfig(std::shared_ptr<const Config::TimeoutConfig> cfg);

private:
    uint32_t getTimeoutSeconds(Type type) const;

    asio::io_context& m_io;
    std::mutex m_mutex;

    struct TimerEntry {
        std::unique_ptr<asio::steady_timer> timer;
        std::function<void()> callback;
    };

    std::unordered_map<Type, TimerEntry> m_timers;
    Config::TimeoutConfig m_config;
};