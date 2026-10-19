#include <gtest/gtest.h>
#include "RateLimiter.h"
#include "Config.h"
#include <memory>

static void setupRateLimit(uint64_t globalUp, uint64_t globalDown,
                           const std::map<std::string, Config::UserRateLimit>& users = {}) {
    Config::RateLimitConfig cfg;
    cfg.globalUpLimit = globalUp;
    cfg.globalDownLimit = globalDown;
    cfg.userLimits = users;
    RateLimiter::getInstance().refreshConfig(
        std::make_shared<const Config::RateLimitConfig>(cfg));
}

TEST(RateLimiterTest, NoLimitAllowsAll) {
    setupRateLimit(0, 0);
    auto r = RateLimiter::getInstance().acquire("user1", 1000000, true);
    EXPECT_EQ(r.immediateBytes, 1000000u);
}

TEST(RateLimiterTest, GlobalUpLimit) {
    setupRateLimit(1000, 0);
    auto r1 = RateLimiter::getInstance().acquire("user1", 600, true);
    EXPECT_EQ(r1.immediateBytes, 600u);
    auto r2 = RateLimiter::getInstance().acquire("user2", 600, true);
    EXPECT_EQ(r2.immediateBytes, 500u);
    auto r3 = RateLimiter::getInstance().acquire("user3", 100, true);
    EXPECT_EQ(r3.immediateBytes, 0u);
}

TEST(RateLimiterTest, GlobalDownLimit) {
    setupRateLimit(0, 1000);
    auto r1 = RateLimiter::getInstance().acquire("user1", 600, false);
    EXPECT_EQ(r1.immediateBytes, 600u);
    auto r2 = RateLimiter::getInstance().acquire("user2", 600, false);
    EXPECT_EQ(r2.immediateBytes, 500u);
}

TEST(RateLimiterTest, UserLevelLimit) {
    Config::UserRateLimit ul;
    ul.upLimit = 500;
    ul.downLimit = 0;
    setupRateLimit(0, 0, {{"user1", ul}});
    auto r1 = RateLimiter::getInstance().acquire("user1", 300, true);
    EXPECT_EQ(r1.immediateBytes, 300u);
    auto r2 = RateLimiter::getInstance().acquire("user1", 300, true);
    EXPECT_EQ(r2.immediateBytes, 250u);
    auto r3 = RateLimiter::getInstance().acquire("user2", 1000, true);
    EXPECT_EQ(r3.immediateBytes, 1000u);
}

TEST(RateLimiterTest, GlobalAndUserCombined) {
    Config::UserRateLimit ul;
    ul.upLimit = 500;
    ul.downLimit = 0;
    setupRateLimit(1000, 0, {{"user1", ul}});
    auto r1 = RateLimiter::getInstance().acquire("user1", 400, true);
    EXPECT_EQ(r1.immediateBytes, 400u);
    auto r2 = RateLimiter::getInstance().acquire("user1", 200, true);
    EXPECT_EQ(r2.immediateBytes, 150u);
}