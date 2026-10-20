#include "NatTraversalManager.h"
#include "Logger.h"


void NatTraversalManager::enable(asio::io_context& io, std::shared_ptr<const Config::NatTraversalConfig> cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_io = &io;
    m_config = cfg;
    m_enabled = true;

    m_probe_io = std::make_unique<asio::io_context>();
    probeNow();
    startKeepalive();

    Logger::getInstance().info("[NAT] Enabled, STUN server=" + cfg->stunServer + ":" + std::to_string(cfg->stunPort));
}

void NatTraversalManager::disable() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = false;
    if (m_timer) {
        m_timer->cancel();
        m_timer.reset();
    }
    if (m_probe_io) {
        m_probe_io->stop();
        m_probe_io.reset();
    }
    if (m_probe_thread && m_probe_thread->joinable()) {
        m_probe_thread->join();
        m_probe_thread.reset();
    }
    Logger::getInstance().info("[NAT] Disabled");
}

std::optional<NatInfo> NatTraversalManager::probeNow() {
    if (!m_config || !m_probe_io) return std::nullopt;

    StunClient client(*m_probe_io);
    auto info = client.probe(m_config->stunServer, m_config->stunPort, 0, 5000);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (info) {
        m_currentState = *info;
        Logger::getInstance().info("[NAT] Probe result: type=" +
            Config::natTypeToString(info->type) + " publicIp=" + info->publicIp +
            ":" + std::to_string(info->publicPort));
    } else {
        Logger::getInstance().warn("[NAT] Probe failed");
    }
    return info;
}

void NatTraversalManager::startKeepalive() {
    if (!m_io || !m_config) return;
    m_timer = std::make_unique<asio::steady_timer>(*m_io);
    onKeepalive();
}

void NatTraversalManager::onKeepalive() {
    if (!m_enabled || !m_timer) return;

    m_timer->expires_after(std::chrono::seconds(m_config->keepaliveSeconds));
    m_timer->async_wait([this](std::error_code ec) {
        if (!ec && m_enabled) {
            probeNow();
            onKeepalive();
        }
    });
}

NatInfo NatTraversalManager::getCurrentState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentState;
}

bool NatTraversalManager::isEnabled() const {
    return m_enabled.load();
}