#include <gtest/gtest.h>
#include "ProtocolDetector.h"
#include <cstring>

TEST(ProtocolDetectorTest, HttpConnect) {
    const char* req = "CONNECT example.com:443 HTTP/1.1\r\n";
    ProtocolDetector det;
    det.feed(reinterpret_cast<const uint8_t*>(req), strlen(req));
    EXPECT_EQ(det.detect(), ProtocolType::HTTP_CONNECT);
    EXPECT_FALSE(det.needsMoreData());
}

TEST(ProtocolDetectorTest, HttpGet) {
    const char* req = "GET / HTTP/1.1\r\nHost: example.com\r\n";
    ProtocolDetector det;
    det.feed(reinterpret_cast<const uint8_t*>(req), strlen(req));
    EXPECT_EQ(det.detect(), ProtocolType::HTTP_CONNECT);
}

TEST(ProtocolDetectorTest, Socks5Tcp) {
    uint8_t data[] = {0x05, 0x01, 0x00};
    ProtocolDetector det;
    det.feed(data, 3);
    EXPECT_EQ(det.detect(), ProtocolType::SOCKS5_TCP);
}

TEST(ProtocolDetectorTest, Socks5DetectedFromFirstByte) {
    uint8_t data[] = {0x05, 0x01, 0x00, 0x03, 0x00};
    ProtocolDetector det;
    det.feed(data, 5);
    EXPECT_EQ(det.detect(), ProtocolType::SOCKS5_TCP);
}

TEST(ProtocolDetectorTest, Socks4) {
    uint8_t data[] = {0x04, 0x01, 0x00, 0x50, 0x01, 0x02, 0x03, 0x04};
    ProtocolDetector det;
    det.feed(data, 8);
    EXPECT_EQ(det.detect(), ProtocolType::SOCKS4);
}

TEST(ProtocolDetectorTest, Socks4aDetectedAsSocks4) {
    uint8_t data[] = {0x04, 0x01, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01};
    ProtocolDetector det;
    det.feed(data, 8);
    EXPECT_EQ(det.detect(), ProtocolType::SOCKS4);
}

TEST(ProtocolDetectorTest, WebSocketUpgrade) {
    const char* req = "GET /ws HTTP/1.1\r\nUpgrade: websocket\r\n";
    ProtocolDetector det;
    det.feed(reinterpret_cast<const uint8_t*>(req), strlen(req));
    EXPECT_EQ(det.detect(), ProtocolType::WEBSOCKET);
}

TEST(ProtocolDetectorTest, UnknownProtocol) {
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    ProtocolDetector det;
    det.feed(data, 4);
    EXPECT_EQ(det.detect(), ProtocolType::UNKNOWN);
}

TEST(ProtocolDetectorTest, NeedsMoreDataInitially) {
    ProtocolDetector det;
    EXPECT_TRUE(det.needsMoreData());
}

TEST(ProtocolDetectorTest, PartialFeedThenComplete) {
    const char* part1 = "CONNECT ";
    const char* part2 = "example.com:443 HTTP/1.1\r\n";
    ProtocolDetector det;
    det.feed(reinterpret_cast<const uint8_t*>(part1), strlen(part1));
    det.feed(reinterpret_cast<const uint8_t*>(part2), strlen(part2));
    EXPECT_EQ(det.detect(), ProtocolType::HTTP_CONNECT);
}

TEST(ProtocolDetectorTest, ExceededLimit) {
    std::vector<uint8_t> large(ProtocolDetector::MAX_BUFFER + 1, 0xAA);
    ProtocolDetector det;
    det.feed(large.data(), large.size());
    EXPECT_TRUE(det.exceededLimit());
}

TEST(ProtocolDetectorTest, NotExceededLimit) {
    uint8_t data[] = {0x05, 0x01, 0x00};
    ProtocolDetector det;
    det.feed(data, 3);
    EXPECT_FALSE(det.exceededLimit());
}

TEST(ProtocolDetectorTest, ProtocolTypeToString) {
    EXPECT_STREQ(protocolTypeToString(ProtocolType::HTTP_CONNECT), "HTTP CONNECT");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::SOCKS5_TCP), "SOCKS5 TCP");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::SOCKS4), "SOCKS4");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::WEBSOCKET), "WebSocket");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::UNKNOWN), "Unknown");
}