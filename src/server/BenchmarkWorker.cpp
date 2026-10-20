#include "BenchmarkWorker.h"
#include <cstring>

using asio::ip::tcp;

BenchmarkWorker::BenchmarkWorker(asio::io_context& io, const std::string& protocol,
                                 const std::string& host, uint16_t port)
    : m_io(io), m_protocol(protocol), m_host(host), m_port(port),
      m_socket(io), m_write_buf(1024, 'A'), m_read_buf(65536) {
}

void BenchmarkWorker::run() {
    m_start_time = std::chrono::steady_clock::now();
    doConnect();
}

void BenchmarkWorker::stop() {
    m_running = false;
    std::error_code ec;
    m_socket.close(ec);
}

void BenchmarkWorker::doConnect() {
    auto self = this;
    tcp::resolver resolver(m_io);
    auto results = resolver.resolve(m_host, std::to_string(m_port));
    asio::async_connect(m_socket, results,
        [this, self](std::error_code ec, tcp::endpoint) {
            if (!ec) {
                auto elapsed = std::chrono::steady_clock::now() - m_start_time;
                m_handshake_latency_ms = std::chrono::duration<double, std::milli>(elapsed).count();
                doWrite();
            } else {
                m_done = true;
            }
        });
}

void BenchmarkWorker::doWrite() {
    if (!m_running) { m_done = true; return; }
    auto self = this;
    asio::async_write(m_socket, asio::buffer(m_write_buf),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_up += length;
                doRead();
            } else {
                m_done = true;
            }
        });
}

void BenchmarkWorker::doRead() {
    auto self = this;
    m_socket.async_read_some(asio::buffer(m_read_buf),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                m_bytes_down += length;
                doWrite();
            } else {
                m_done = true;
            }
        });
}