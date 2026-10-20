#pragma once

#include <cstdint>
#include <memory>
#include <stack>
#include <mutex>
#include <atomic>

class BufferPool {
public:
    static BufferPool& getInstance() {
        static BufferPool instance;
        return instance;
    }

    std::shared_ptr<uint8_t[]> acquire(std::size_t size);
    void release(std::shared_ptr<uint8_t[]> buf);

    std::size_t poolSize() const;
    std::size_t totalAllocated() const { return m_total_allocated.load(); }

    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 65536;

private:
    BufferPool() = default;

    struct BufferEntry {
        std::size_t size;
        std::unique_ptr<uint8_t[]> data;
    };

    mutable std::mutex m_mutex;
    std::stack<BufferEntry> m_pool;
    std::atomic<std::size_t> m_total_allocated{0};
};