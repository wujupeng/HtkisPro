#include "BufferPool.h"

std::shared_ptr<uint8_t[]> BufferPool::acquire(std::size_t size) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_pool.empty()) {
            auto entry = std::move(m_pool.top());
            m_pool.pop();
            if (entry.size >= size) {
                auto raw = entry.data.release();
                return std::shared_ptr<uint8_t[]>(raw, [this](uint8_t* p) {
                    if (!p) return;
                    std::lock_guard<std::mutex> lk(m_mutex);
                    m_pool.push({DEFAULT_BUFFER_SIZE, std::unique_ptr<uint8_t[]>(p)});
                });
            }
        }
    }
    m_total_allocated.fetch_add(1);
    auto raw = new uint8_t[size]();
    return std::shared_ptr<uint8_t[]>(raw, [this](uint8_t* p) {
        if (!p) return;
        std::lock_guard<std::mutex> lk(m_mutex);
        m_pool.push({DEFAULT_BUFFER_SIZE, std::unique_ptr<uint8_t[]>(p)});
    });
}

void BufferPool::release(std::shared_ptr<uint8_t[]> buf) {

}

std::size_t BufferPool::poolSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}
