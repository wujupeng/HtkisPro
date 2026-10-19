#include "DeadlineTimer.h"

DeadlineTimer::DeadlineTimer(asio::io_context& io)
    : m_io(io), m_config(Config::defaultConfig().timeout) {
}

uint32_t DeadlineTimer::getTimeoutSeconds(Type type) const {
    switch (type) {
        case Type::FirstPacket: return m_config.firstPacketSeconds;
        case Type::Connect: return m_config.connectSeconds;
        case Type::Idle: return m_config.idleSeconds;
        case Type::UdpAssociate: return m_config.idleSeconds;
        default: return 300;
    }
}

void DeadlineTimer::start(Type type, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& entry = m_timers[type];
    entry.callback = std::move(callback);
    entry.timer = std::make_unique<asio::steady_timer>(m_io);
    entry.timer->expires_after(std::chrono::seconds(getTimeoutSeconds(type)));

    auto cb = entry.callback;
    entry.timer->async_wait([this, type, cb](const std::error_code& ec) {
        if (!ec) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_timers.find(type);
            if (it != m_timers.end() && it->second.callback) {
                it->second.callback();
            }
        }
    });
}

void DeadlineTimer::cancel(Type type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_timers.find(type);
    if (it != m_timers.end() && it->second.timer) {
        it->second.timer->cancel();
        it->second.callback = nullptr;
    }
}

void DeadlineTimer::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [type, entry] : m_timers) {
        if (entry.timer) {
            std::error_code ec;
            entry.timer->cancel();
            entry.callback = nullptr;
        }
    }
}

void DeadlineTimer::resetIdle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_timers.find(Type::Idle);
    if (it != m_timers.end() && it->second.timer) {
        it->second.timer->expires_after(std::chrono::seconds(getTimeoutSeconds(Type::Idle)));
        auto cb = it->second.callback;
        it->second.timer->async_wait([this, cb](const std::error_code& ec) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it2 = m_timers.find(Type::Idle);
                if (it2 != m_timers.end() && it2->second.callback) {
                    it2->second.callback();
                }
            }
        });
    }
}

void DeadlineTimer::refreshConfig(std::shared_ptr<const Config::TimeoutConfig> cfg) {
    if (!cfg) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = *cfg;
}