#include <gtest/gtest.h>
#include "VmessHeaderDecoder.h"
#include <cstring>

TEST(VmessHeaderDecoderTest, ParseValidHeader) {
    std::vector<uint8_t> data;
    data.push_back(0x01);
    for (int i = 0; i < 16; ++i) data.push_back(static_cast<uint8_t>(i));
    data.push_back(0);
    data.push_back(0x01);
    data.push_back(0x00);
    data.push_back(0x50);
    data.push_back(0x01);
    data.push_back(127); data.push_back(0); data.push_back(0); data.push_back(1);
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);

    auto header = VmessHeaderDecoder::decode(data.data(), data.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->version, 0x01);
    EXPECT_EQ(header->command, 0x01);
    EXPECT_EQ(header->port, 0x0050);
    EXPECT_EQ(header->addrType, 0x01);
    EXPECT_EQ(header->host, "127.0.0.1");
}

TEST(VmessHeaderDecoderTest, ParseDomainAddress) {
    std::vector<uint8_t> data;
    data.push_back(0x01);
    for (int i = 0; i < 16; ++i) data.push_back(0xAA);
    data.push_back(0);
    data.push_back(0x01);
    data.push_back(0x01); data.push_back(0xBB);
    data.push_back(0x02);
    const char* domain = "example.com";
    data.push_back(static_cast<uint8_t>(strlen(domain)));
    for (size_t i = 0; i < strlen(domain); ++i) data.push_back(domain[i]);
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);

    auto header = VmessHeaderDecoder::decode(data.data(), data.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->addrType, 0x02);
    EXPECT_EQ(header->host, "example.com");
}

TEST(VmessHeaderDecoderTest, RejectTooShort) {
    std::vector<uint8_t> data = {0x01, 0x02};
    auto header = VmessHeaderDecoder::decode(data.data(), data.size());
    EXPECT_FALSE(header.has_value());
}