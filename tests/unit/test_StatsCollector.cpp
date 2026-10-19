#include <gtest/gtest.h>
#include "StatsCollector.h"

TEST(StatsCollectorTest, SessionStartIncrement) {
    auto& sc = StatsCollector::getInstance();
    uint32_t before = sc.getActiveSessionCount();
    sc.onSessionStart(100001, "user1", "HTTP", "example.com:80", "127.0.0.1");
    EXPECT_EQ(sc.getActiveSessionCount(), before + 1);
    sc.onSessionClose(100001, "test");
    EXPECT_EQ(sc.getActiveSessionCount(), before);
}

TEST(StatsCollectorTest, SessionUpdateBytes) {
    auto& sc = StatsCollector::getInstance();
    sc.onSessionStart(200001, "user2", "SOCKS5", "target:443", "10.0.0.1");
    sc.onSessionUpdate(200001, 1000, 2000);
    auto snap = sc.getSnapshot();
    bool found = false;
    for (auto& s : snap.activeSessions) {
        if (s.sessionId == 200001) {
            EXPECT_EQ(s.bytesUp, 1000u);
            EXPECT_EQ(s.bytesDown, 2000u);
            found = true;
        }
    }
    EXPECT_TRUE(found);
    sc.onSessionClose(200001, "test");
}

TEST(StatsCollectorTest, MultipleSessions) {
    auto& sc = StatsCollector::getInstance();
    uint32_t before = sc.getActiveSessionCount();
    sc.onSessionStart(300001, "a", "HTTP", "t1:80", "1.1.1.1");
    sc.onSessionStart(300002, "b", "SOCKS5", "t2:80", "2.2.2.2");
    sc.onSessionStart(300003, "c", "WS", "t3:80", "3.3.3.3");
    EXPECT_EQ(sc.getActiveSessionCount(), before + 3);
    sc.onSessionClose(300002, "done");
    EXPECT_EQ(sc.getActiveSessionCount(), before + 2);
    sc.onSessionClose(300001, "done");
    sc.onSessionClose(300003, "done");
    EXPECT_EQ(sc.getActiveSessionCount(), before);
}

TEST(StatsCollectorTest, SnapshotContainsSessionInfo) {
    auto& sc = StatsCollector::getInstance();
    sc.onSessionStart(400001, "userX", "SOCKS4", "target:1080", "192.168.1.5");
    auto snap = sc.getSnapshot();
    bool found = false;
    for (auto& s : snap.activeSessions) {
        if (s.sessionId == 400001) {
            EXPECT_EQ(s.username, "userX");
            EXPECT_EQ(s.protocol, "SOCKS4");
            EXPECT_EQ(s.target, "target:1080");
            EXPECT_EQ(s.clientAddr, "192.168.1.5");
            EXPECT_TRUE(s.active);
            found = true;
        }
    }
    EXPECT_TRUE(found);
    sc.onSessionClose(400001, "test");
}

TEST(StatsCollectorTest, GlobalByteCounting) {
    auto& sc = StatsCollector::getInstance();
    auto snapBefore = sc.getSnapshot();
    sc.onSessionStart(500001, "u", "HTTP", "t:80", "1.1.1.1");
    sc.onSessionUpdate(500001, 5000, 3000);
    auto snapAfter = sc.getSnapshot();
    EXPECT_GT(snapAfter.globalBytesUp, snapBefore.globalBytesUp);
    EXPECT_GT(snapAfter.globalBytesDown, snapBefore.globalBytesDown);
    sc.onSessionClose(500001, "test");
}

TEST(StatsCollectorTest, CompactDoesNotCrash) {
    auto& sc = StatsCollector::getInstance();
    sc.onSessionStart(600001, "u", "HTTP", "t:80", "1.1.1.1");
    sc.onSessionClose(600001, "done");
    sc.compact();
    SUCCEED();
}