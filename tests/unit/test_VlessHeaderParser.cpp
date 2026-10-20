#include <gtest/gtest.h>
#include "VlessHeaderParser.h"

TEST(VlessHeaderParserTest, ParseValidHeader) {
    std::vector<uint8_t> data;
    data.push_back(0x00);
    for (int i = 0; i < 16; ++i) data.push_back(static_cast<uint8_t>(i));
    data.push_back(0);
    data.push_back(0x01);
    data.push_back(0x00); data.push_back(0x50);
    data.push_back(0x01);
    data.push_back(127); data.push_back(0); data.push_back(0); data.push_back(1);

    auto header = VlessHeaderParser::parse(data.data(), data.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->version, 0x00);
    EXPECT_EQ(header->command, 0x01);
    EXPECT_EQ(header->port, 0x0050);
    EXPECT_EQ(header->host, "127.0.0.1");
}

TEST(VlessHeaderParserTest, ParseDomainAddress) {
    std::vector<uint8_t> data;
    data.push_back(0x00);
    for (int i = 0; i < 16; ++i) data.push_back(0xBB);
    data.push_back(0);
    data.push_back(0x01);
    data.push_back(0x01); data.push_back(0xBB);
    data.push_back(0x02);
    const char* domain = "test.com";
    data.push_back(static_cast<uint8_t>(strlen(domain)));
    for (size_t i = 0; i < strlen(domain); ++i) data.push_back(domain[i]);

    auto header = VlessHeaderParser::parse(data.data(), data.size());
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->host, "test.com");
}

TEST(VlessHeaderParserTest, BuildResponse) {
    auto resp = VlessHeaderParser::buildResponse(0x00);
    ASSERT_EQ(resp.size(), 2);
    EXPECT_EQ(resp[0], 0x00);
    EXPECT_EQ(resp[1], 0x00);
}

TEST(VlessHeaderParserTest, RejectTooShort) {
    std::vector<uint8_t> data = {0x00};
    auto header = VlessHeaderParser::parse(data.data(), data.size());
    EXPECT_FALSE(header.has_value());
}