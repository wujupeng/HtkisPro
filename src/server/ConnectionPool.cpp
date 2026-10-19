#include "ConnectionPool.h"
#include "Logger.h"

ConnectionPool::PoolKey ConnectionPool::makeKey(const std::string& host, uint16_t port, const std::string& username) const {
    PoolKey key;
    key.host = host;
    key.port = port;
    key.username = m_config.crossUserReuse ? "" : username;
    return key;
}

bool ConnectionPool::isSocketHealthy(asio::ip::tcp::socket& socket) const {
    std::error_code ec;
    auto ep = socket.remote_endpoint(ec);
    return !ec;
}

void ConnectionPool::evictExpired() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_pool.begin(); it != m_pool.end();) {
        auto& queue = it->second;
        while (!queue.empty()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - queue.front().lastUsed).count();
            if (elapsed >= m_config.idleTtlSeconds) {
                std::error_code ec;
                queue.front().socket.close(ec);
                queue.pop_front();
            } else {
                break;
            }
        }
        if (queue.empty()) {
            it = m_pool.erase(it);
        } else {
            ++it;
        }
    }
}

void ConnectionPool::evictLRU() {
    size_t total = 0;
    for (const auto& [key, queue] : m_pool) {
        total += queue.size();
    }
    while (total > m_config.maxConnections && !m_lru.empty()) {
        auto& oldestKey = m_lru.back();
        auto it = m_pool.find(oldestKey);
        if (it != m_pool.end() && !it->second.empty()) {
            std::error_code ec;
            it->second.front().socket.close(ec);
            it->second.pop_front();
            total--;
            if (it->second.empty()) {
                m_pool.erase(it);
                m_lruIndex.erase(oldestKey);
                m_lru.pop_back();
            }
        } else {
            m_lruIndex.erase(oldestKey);
            m_lru.pop_back();
        }
    }
}

std::optional<asio::ip::tcp::socket> ConnectionPool::acquire(
    asio::io_context& io,
    const std::string& host, uint16_t port,
    const std::string& username) {

    if (!m_config.enabled) return std::nullopt;

    std::lock_guard<std::mutex> lock(m_mutex);
    evictExpired();

    PoolKey key = makeKey(host, port, username);
    auto it = m_pool.find(key);
    if (it == m_pool.end() || it->second.empty()) return std::nullopt;

    while (!it->second.empty()) {
        auto& entry = it->second.front();
        if (isSocketHealthy(entry.socket)) {
            auto socket = std::move(entry.socket);
            it->second.pop_front();
            return std::make_optional(std::move(socket));
        }
        std::error_code ec;
        entry.socket.close(ec);
        it->second.pop_front();
    }
    return std::nullopt;
}

void ConnectionPool::release(
    asio::ip::tcp::socket socket,
    const std::string& host, uint16_t port,
    const std::string& username) {

    if (!m_config.enabled) {
        std::error_code ec;
        socket.close(ec);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!isSocketHealthy(socket)) {
        std::error_code ec;
        socket.close(ec);
        return;
    }

    PoolKey key = makeKey(host, port, username);
    m_pool[key].emplace_back(std::move(socket), std::chrono::steady_clock::now());

    evictLRU();
}

void ConnectionPool::refreshConfig(std::shared_ptr<const Config::ConnectionPoolConfig> cfg) {
    if (!cfg) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = *cfg;
}

size_t ConnectionPool::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& [key, queue] : m_pool) {
        total += queue.size();
    }
    return total;
}

void ConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [key, queue] : m_pool) {
        for (auto& entry : queue) {
            std::error_code ec;
            entry.socket.close(ec);
        }
    }
    m_pool.clear();
    m_lru.clear();
    m_lruIndex.clear();
}