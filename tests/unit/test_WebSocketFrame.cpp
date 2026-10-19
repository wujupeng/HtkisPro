#include <gtest/gtest.h>
#include "WebSocketFrame.h"
#include <string>

TEST(WebSocketFrameTest, EncodeDecodeTextSmall) {
    std::string msg = "Hello";
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Text, msg, false);
    ASSERT_FALSE(encoded.empty());
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->fin);
    EXPECT_EQ(frame->opcode, WebSocketOpcode::Text);
    EXPECT_EQ(frame->payload.size(), msg.size());
    EXPECT_EQ(std::string(frame->payload.begin(), frame->payload.end()), msg);
    EXPECT_EQ(consumed, encoded.size());
}

TEST(WebSocketFrameTest, EncodeDecodeBinary) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Binary, data.data(), data.size(), false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, WebSocketOpcode::Binary);
    EXPECT_EQ(frame->payload, data);
}

TEST(WebSocketFrameTest, EncodeDecodeMasked) {
    std::string msg = "Masked message";
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Text, msg, true);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(std::string(frame->payload.begin(), frame->payload.end()), msg);
}

TEST(WebSocketFrameTest, EncodePing) {
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Ping, "ping", false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, WebSocketOpcode::Ping);
}

TEST(WebSocketFrameTest, EncodePong) {
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Pong, "pong", false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, WebSocketOpcode::Pong);
}

TEST(WebSocketFrameTest, EncodeClose) {
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Close, "", false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->opcode, WebSocketOpcode::Close);
}

TEST(WebSocketFrameTest, LargePayload) {
    std::string large(1000, 'X');
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Text, large, false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->payload.size(), large.size());
}

TEST(WebSocketFrameTest, EmptyPayload) {
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Text, "", false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), encoded.size(), consumed);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->payload.size(), 0u);
}

TEST(WebSocketFrameTest, PartialDecodeReturnsNullopt) {
    std::string msg = "Hello World";
    auto encoded = WebSocketFrameCoder::encode(WebSocketOpcode::Text, msg, false);
    std::size_t consumed = 0;
    auto frame = WebSocketFrameCoder::decode(encoded.data(), 2, consumed);
    EXPECT_FALSE(frame.has_value());
    EXPECT_EQ(consumed, 0u);
}

TEST(WebSocketFrameTest, ComputeAcceptKey) {
    std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string accept = WebSocketFrameCoder::computeAcceptKey(key);
    EXPECT_EQ(accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WebSocketFrameTest, MultipleFramesInBuffer) {
    auto f1 = WebSocketFrameCoder::encode(WebSocketOpcode::Text, "first", false);
    auto f2 = WebSocketFrameCoder::encode(WebSocketOpcode::Text, "second", false);
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), f1.begin(), f1.end());
    combined.insert(combined.end(), f2.begin(), f2.end());

    std::size_t consumed1 = 0;
    auto frame1 = WebSocketFrameCoder::decode(combined.data(), combined.size(), consumed1);
    ASSERT_TRUE(frame1.has_value());
    EXPECT_EQ(std::string(frame1->payload.begin(), frame1->payload.end()), "first");

    std::size_t consumed2 = 0;
    auto frame2 = WebSocketFrameCoder::decode(combined.data() + consumed1, combined.size() - consumed1, consumed2);
    ASSERT_TRUE(frame2.has_value());
    EXPECT_EQ(std::string(frame2->payload.begin(), frame2->payload.end()), "second");
}