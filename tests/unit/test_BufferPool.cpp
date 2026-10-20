#include <gtest/gtest.h>
#include "BufferPool.h"

TEST(BufferPoolTest, AcquireReturnsValidBuffer) {
    auto& pool = BufferPool::getInstance();
    auto buf = pool.acquire(BufferPool::DEFAULT_BUFFER_SIZE);
    ASSERT_NE(buf, nullptr);
}

TEST(BufferPoolTest, AcquireDifferentSizes) {
    auto& pool = BufferPool::getInstance();
    auto buf1 = pool.acquire(1024);
    auto buf2 = pool.acquire(65536);
    EXPECT_NE(buf1, nullptr);
    EXPECT_NE(buf2, nullptr);
}

TEST(BufferPoolTest, PoolSizeAfterRelease) {
    auto& pool = BufferPool::getInstance();
    auto buf = pool.acquire(BufferPool::DEFAULT_BUFFER_SIZE);
    ASSERT_NE(buf, nullptr);
    auto sizeBefore = pool.poolSize();
    pool.release(std::move(buf));
    EXPECT_GE(pool.poolSize(), sizeBefore);
}

TEST(BufferPoolTest, MultipleAcquire) {
    auto& pool = BufferPool::getInstance();
    std::vector<std::shared_ptr<uint8_t[]>> bufs;
    for (int i = 0; i < 10; ++i) {
        bufs.push_back(pool.acquire(BufferPool::DEFAULT_BUFFER_SIZE));
        EXPECT_NE(bufs.back(), nullptr);
    }
}

TEST(BufferPoolTest, TotalAllocatedIncreases) {
    auto& pool = BufferPool::getInstance();
    auto before = pool.totalAllocated();
    pool.acquire(BufferPool::DEFAULT_BUFFER_SIZE);
    EXPECT_GE(pool.totalAllocated(), before);
}