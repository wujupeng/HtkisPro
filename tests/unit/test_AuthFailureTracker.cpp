#include <gtest/gtest.h>
#include "AuthFailureTracker.h"

TEST(AuthFailureTrackerTest, NotBlockedInitially) {
    EXPECT_FALSE(AuthFailureTracker::getInstance().isBlocked("192.168.1.1"));
}

TEST(AuthFailureTrackerTest, BlockedAfterThreshold) {
    auto& t = AuthFailureTracker::getInstance();
    for (int i = 0; i < AuthFailureTracker::FAILURE_THRESHOLD; i++) {
        t.recordFailure("10.0.0.99");
    }
    EXPECT_TRUE(t.isBlocked("10.0.0.99"));
}

TEST(AuthFailureTrackerTest, NotBlockedBeforeThreshold) {
    auto& t = AuthFailureTracker::getInstance();
    for (int i = 0; i < AuthFailureTracker::FAILURE_THRESHOLD - 1; i++) {
        t.recordFailure("10.0.0.88");
    }
    EXPECT_FALSE(t.isBlocked("10.0.0.88"));
}

TEST(AuthFailureTrackerTest, DifferentIpIndependent) {
    auto& t = AuthFailureTracker::getInstance();
    for (int i = 0; i < AuthFailureTracker::FAILURE_THRESHOLD; i++) {
        t.recordFailure("172.16.0.1");
    }
    EXPECT_TRUE(t.isBlocked("172.16.0.1"));
    EXPECT_FALSE(t.isBlocked("172.16.0.2"));
}

TEST(AuthFailureTrackerTest, GetBlockedReturnsList) {
    auto& t = AuthFailureTracker::getInstance();
    for (int i = 0; i < AuthFailureTracker::FAILURE_THRESHOLD; i++) {
        t.recordFailure("192.168.99.1");
    }
    auto blocked = t.getBlocked();
    bool found = false;
    for (auto& [ip, count] : blocked) {
        if (ip == "192.168.99.1") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(AuthFailureTrackerTest, SingleFailureNotBlocked) {
    auto& t = AuthFailureTracker::getInstance();
    t.recordFailure("10.1.1.1");
    EXPECT_FALSE(t.isBlocked("10.1.1.1"));
}

TEST(AuthFailureTrackerTest, MultipleIpsBlocked) {
    auto& t = AuthFailureTracker::getInstance();
    for (int i = 0; i < AuthFailureTracker::FAILURE_THRESHOLD; i++) {
        t.recordFailure("10.2.2.2");
        t.recordFailure("10.3.3.3");
    }
    EXPECT_TRUE(t.isBlocked("10.2.2.2"));
    EXPECT_TRUE(t.isBlocked("10.3.3.3"));
}