#include "CryptoSessionBase.h"
#include "Logger.h"
#include "IpAcl.h"
#include "ConfigManager.h"
#include "StatsCollector.h"
#include "Server.h"
#include <cstring>

using asio::ip::tcp;

static std::string sessionEndReasonToString(SessionEndReason reason) {
    switch (reason) {
        case SessionEndReason::ClientDisconnect:    return "client_disconnect";
        case SessionEndReason::RemoteDisconnect:    return "remote_disconnect";
        case SessionEndReason::Timeout:             return "timeout";
        case SessionEndReason::AuthFailed:          return "auth_failed";
        case SessionEndReason::ConnectFailed:       return "connect_failed";
        case SessionEndReason::AclDenied:           return "acl_denied";
        case SessionEndReason::RateLimitExceeded:   return "rate_limit";
        case SessionEndReason::RelayDisconnect:     return "relay_disconnect";
        case SessionEndReason::Error:               return "error";
        default:                                    return "unknown";
    }
}

static std::string formatBytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

static std::string formatDuration(std::time_t seconds) {
    if (seconds < 60) return std::to_string(seconds) + "s";
    if (seconds < 3600) return std::to_string(seconds / 60) + "m " + std::to_string(seconds % 60) + "s";
    char buf[32];
    snprintf(buf, sizeof(buf), "%lldh %lldm", (long long)(seconds / 3600), (long long)((seconds % 3600) / 60));
    return buf;
}

CryptoSessionBase::CryptoSessionBase(tcp::socket socket)
    : m_client_socket(std::move(socket)),
      m_remote_socket(static_cast<asio::io_context&>(m_client_socket.get_executor().context())),
      m_resolver(static_cast<asio::io_context&>(m_client_socket.get_executor().context())),
      m_idle_timer(static_cast<asio::io_context&>(m_client_socket.get_executor().context())) {
}

CryptoSessionBase::~CryptoSessionBase() {
    zeroSessionKey();
}

void CryptoSessionBase::zeroSessionKey() {
    if (!m_sessionKey.empty()) {
        std::fill(m_sessionKey.begin(), m_sessionKey.end(), static_cast<uint8_t>(0));
    }
}

void CryptoSessionBase::start() {
    auto ep = m_client_socket.remote_endpoint();
    m_client_addr = ep.address().to_string() + ":" + std::to_string(ep.port());
    m_start_time = std::time(nullptr);
    Server::activeSessionCount().fetch_add(1);

    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    IpAcl::getInstance().refreshConfig(
        std::make_shared<Config::IpAclConfig>(cfg->ipAcl));
    if (!IpAcl::getInstance().check(ep.address().to_string())) {
        Logger::getInstance().info("[ACL] Rejected " + m_protocol_name + " connection from " + ep.address().to_string());
        closeSession(SessionEndReason::AclDenied);
        return;
    }

    m_idle_timeout_seconds = cfg->timeout.idleSeconds;
    Logger::getInstance().info("[" + m_protocol_name + "] New connection from " + m_client_addr);
    resetIdleTimer();
    onHandshake();
}

void CryptoSessionBase::resetIdleTimer() {
    auto self(shared_from_this());
    m_idle_timer.expires_after(std::chrono::seconds(m_idle_timeout_seconds));
    m_idle_timer.async_wait(
        [this, self](std::error_code ec) {
            onIdleTimeout(std::move(ec));
        });
}

void CryptoSessionBase::onIdleTimeout(std::error_code ec) {
    if (!ec) {
        Logger::getInstance().warn("[" + m_protocol_name + "] Idle timeout from " + m_client_addr);
        closeSession(SessionEndReason::Timeout);
    }
}

void CryptoSessionBase::closeSession(SessionEndReason reason) {
    std::error_code ec;
    m_idle_timer.cancel();
    m_client_socket.close(ec);
    m_remote_socket.close(ec);

    std::string reasonStr = sessionEndReasonToString(reason);
    if (m_tunnel_active) {
        std::time_t duration = std::time(nullptr) - m_start_time;
        Logger::getInstance().info("[" + m_protocol_name + "] CLOSE " + m_client_addr +
            " target=" + m_target_host + ":" + std::to_string(m_target_port) +
            " reason=" + reasonStr +
            " up=" + formatBytes(m_bytes_up.load()) +
            " down=" + formatBytes(m_bytes_down.load()) +
            " duration=" + formatDuration(duration));
    } else {
        Logger::getInstance().info("[" + m_protocol_name + "] CLOSE " + m_client_addr + " reason=" + reasonStr);
    }
    m_tunnel_active = false;
    Server::activeSessionCount().fetch_sub(1);
}

std::vector<uint8_t> CryptoSessionBase::encryptToClient(const uint8_t* data, std::size_t length) {
    return std::vector<uint8_t>(data, data + length);
}

std::optional<std::vector<uint8_t>> CryptoSessionBase::decryptFromClient(const uint8_t* data, std::size_t length) {
    return std::vector<uint8_t>(data, data + length);
}

std::vector<uint8_t> CryptoSessionBase::generateNonce(uint64_t counter, std::size_t nonceLength) {
    std::vector<uint8_t> nonce(nonceLength, 0);
    for (std::size_t i = 0; i < 8 && i < nonceLength; ++i) {
        nonce[i] = static_cast<uint8_t>((counter >> (i * 8)) & 0xFF);
    }
    return nonce;
}

void CryptoSessionBase::doResolve(const std::string& host, uint16_t port) {
    auto self(shared_from_this());
    Logger::getInstance().info("[" + m_protocol_name + "] Resolving " + host + " ...");
    m_resolver.async_resolve(host, std::to_string(port),
        [this, self, host, port](std::error_code ec, tcp::resolver::results_type results) {
            if (!ec) {
                Logger::getInstance().info("[" + m_protocol_name + "] Resolved " + host + " -> " + std::to_string(results.size()) + " endpoint(s)");
                doConnect(results);
            } else {
                Logger::getInstance().error("[" + m_protocol_name + "] DNS resolve failed: " + host + " - " + ec.message());
                closeSession(SessionEndReason::ConnectFailed);
            }
        });
}

void CryptoSessionBase::doConnect(const tcp::resolver::results_type& endpoints) {
    auto self(shared_from_this());
    asio::async_connect(m_remote_socket, endpoints,
        [this, self](std::error_code ec, tcp::endpoint endpoint) {
            if (!ec) {
                m_tunnel_active = true;
                Logger::getInstance().info("[" + m_protocol_name + "] Tunnel established -> " + m_target_host + ":" + std::to_string(m_target_port) + " from=" + m_client_addr + " remote=" + endpoint.address().to_string() + ":" + std::to_string(endpoint.port()));
                doStream();
            } else {
                Logger::getInstance().error("[" + m_protocol_name + "] Connect failed: " + m_target_host + ":" + std::to_string(m_target_port) + " - " + ec.message());
                closeSession(SessionEndReason::ConnectFailed);
            }
        });
}

void CryptoSessionBase::doStream() {
    doReadClient();
    doReadRemote();
}

void CryptoSessionBase::doReadClient() {
    auto self(shared_from_this());
    m_client_socket.async_read_some(asio::buffer(m_client_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_up += length;
                auto decrypted = decryptFromClient(m_client_buffer.data(), length);
                if (decrypted && !decrypted->empty()) {
                    doWriteRemote(decrypted->size());
                    std::memcpy(m_client_buffer.data(), decrypted->data(), decrypted->size());
                } else {
                    closeSession(SessionEndReason::Error);
                }
            } else {
                closeSession(SessionEndReason::ClientDisconnect);
            }
        });
}

void CryptoSessionBase::doWriteRemote(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_remote_socket, asio::buffer(m_client_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadClient();
            } else {
                closeSession(SessionEndReason::Error);
            }
        });
}

void CryptoSessionBase::doReadRemote() {
    auto self(shared_from_this());
    m_remote_socket.async_read_some(asio::buffer(m_remote_buffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_down += length;
                auto encrypted = encryptToClient(m_remote_buffer.data(), length);
                if (!encrypted.empty()) {
                    std::memcpy(m_remote_buffer.data(), encrypted.data(), (std::min)(encrypted.size(), m_remote_buffer.size()));
                    doWriteClient(encrypted.size());
                } else {
                    closeSession(SessionEndReason::Error);
                }
            } else {
                closeSession(SessionEndReason::RemoteDisconnect);
            }
        });
}

void CryptoSessionBase::doWriteClient(std::size_t length) {
    auto self(shared_from_this());
    asio::async_write(m_client_socket, asio::buffer(m_remote_buffer, length),
        [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doReadRemote();
            } else {
                closeSession(SessionEndReason::Error);
            }
        });
}