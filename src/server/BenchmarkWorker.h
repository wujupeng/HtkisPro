#pragma once

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <vector>
#include <chrono>
#include <string>

class BenchmarkWorker {
public:
    BenchmarkWorker(asio::io_context& io, const std::string& protocol,
                    const std::string& host, uint16_t port);

    void run();
    void stop();

    double getHandshakeLatencyMs() const { return m_handshake_latency_ms; }
    uint64_t getBytesUp() const { return m_bytes_up.load(); }
    uint64_t getBytesDown() const { return m_bytes_down.load(); }
    bool isDone() const { return m_done.load(); }

private:
    void doConnect();
    void doWrite();
    void doRead();

    asio::io_context& m_io;
    std::string m_protocol;
    std::string m_host;
    uint16_t m_port;

    asio::ip::tcp::socket m_socket;
    std::vector<uint8_t> m_write_buf;
    std::vector<uint8_t> m_read_buf;

    std::chrono::steady_clock::time_point m_start_time;
    double m_handshake_latency_ms{0.0};
    std::atomic<uint64_t> m_bytes_up{0};
    std::atomic<uint64_t> m_bytes_down{0};
    std::atomic<bool> m_done{false};
    std::atomic<bool> m_running{true};
};