#include <gtest/gtest.h>
#include "VmessChunkCodec.h"
#include "AeadCipher.h"
#include <cstring>

TEST(VmessChunkCodecTest, EncodeDecodeRoundTrip) {
    std::vector<uint8_t> key(16, 0x42);
    auto cipher = AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, key.data());
    VmessChunkCodec codec(std::move(cipher));

    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8};
    auto encoded = codec.encode(plaintext.data(), plaintext.size());
    ASSERT_FALSE(encoded.empty());

    auto decoded = codec.decode(encoded.data(), encoded.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, plaintext);
}

TEST(VmessChunkCodecTest, EmptyInput) {
    std::vector<uint8_t> key(16, 0x42);
    auto cipher = AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, key.data());
    VmessChunkCodec codec(std::move(cipher));

    auto encoded = codec.encode(nullptr, 0);
    EXPECT_TRUE(encoded.empty());
}

TEST(VmessChunkCodecTest, LargePayloadChunking) {
    std::vector<uint8_t> key(16, 0x42);
    auto cipher = AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, key.data());
    VmessChunkCodec codec(std::move(cipher));

    std::vector<uint8_t> plaintext(50000, 0x55);
    auto encoded = codec.encode(plaintext.data(), plaintext.size());
    ASSERT_FALSE(encoded.empty());

    auto decoded = codec.decode(encoded.data(), encoded.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->size(), plaintext.size());
    EXPECT_EQ(std::memcmp(decoded->data(), plaintext.data(), plaintext.size()), 0);
}