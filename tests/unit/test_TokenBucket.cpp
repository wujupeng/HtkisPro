#include <gtest/gtest.h>
#include "TokenBucket.h"
#include <thread>
#include <chrono>

TEST(TokenBucketTest, ZeroRateAllowsAll) {
    TokenBucket tb(0);
    EXPECT_EQ(tb.tryAcquire(100), 100u);
    EXPECT_EQ(tb.tryAcquire(1), 1u);
}

TEST(TokenBucketTest, FullCapacityOnCreate) {
    TokenBucket tb(1000);
    EXPECT_EQ(tb.tryAcquire(1000), 1000u);
}

TEST(TokenBucketTest, PartialAcquire) {
    TokenBucket tb(1000);
    EXPECT_EQ(tb.tryAcquire(600), 600u);
    EXPECT_EQ(tb.tryAcquire(600), 500u);
    EXPECT_EQ(tb.tryAcquire(100), 0u);
}

TEST(TokenBucketTest, RefillAfterTime) {
    TokenBucket tb(1000);
    tb.tryAcquire(1100);
    EXPECT_EQ(tb.tryAcquire(500), 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    tb.refill();
    EXPECT_GT(tb.getTokens(), 0u);
}

TEST(TokenBucketTest, SetRate) {
    TokenBucket tb(1000);
    tb.tryAcquire(1000);
    tb.setRate(5000);
    EXPECT_EQ(tb.getRate(), 5000u);
}

TEST(TokenBucketTest, NoOverAcquire) {
    TokenBucket tb(100);
    EXPECT_EQ(tb.tryAcquire(200), 110u);
    EXPECT_EQ(tb.tryAcquire(100), 0u);
}

TEST(TokenBucketTest, RefillDoesNotExceedCapacity) {
    TokenBucket tb(1000);
    tb.tryAcquire(1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tb.refill();
    EXPECT_LE(tb.getTokens(), 1000u);
}

TEST(TokenBucketTest, MultipleAcquiresAndRefills) {
    TokenBucket tb(10000);
    EXPECT_EQ(tb.tryAcquire(3000), 3000u);
    EXPECT_EQ(tb.tryAcquire(3000), 3000u);
    EXPECT_EQ(tb.tryAcquire(3000), 3000u);
    EXPECT_EQ(tb.tryAcquire(3000), 2000u);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tb.refill();
    EXPECT_GT(tb.getTokens(), 0u);
}