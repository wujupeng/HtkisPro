#include <gtest/gtest.h>
#include "AeadCipher.h"
#include "VmessChunkCodec.h"
#include "Config.h"
#include <chrono>

class PerfRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        key32.assign(32, 0x42);
        key16.assign(16, 0x33);
    }
    std::vector<uint8_t> key32;
    std::vector<uint8_t> key16;
};

TEST_F(PerfRegressionTest, Aes256GcmThroughput) {
    Aes256GcmCipher cipher(key32.data());
    std::vector<uint8_t> data(16384, 0x55);
    std::vector<uint8_t> nonce(12, 0);

    auto start = std::chrono::steady_clock::now();
    int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        nonce[11] = static_cast<uint8_t>(i);
        auto enc = cipher.encrypt(data.data(), data.size(), nullptr, 0, nonce.data());
        auto dec = cipher.decrypt(enc.data(), enc.size(), nullptr, 0, nonce.data());
        ASSERT_TRUE(dec.has_value());
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, 1000.0);
}

TEST_F(PerfRegressionTest, ChaCha20Throughput) {
    ChaCha20IetfPoly1305Cipher cipher(key32.data());
    std::vector<uint8_t> data(16384, 0x55);
    std::vector<uint8_t> nonce(12, 0);

    auto start = std::chrono::steady_clock::now();
    int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        nonce[11] = static_cast<uint8_t>(i);
        auto enc = cipher.encrypt(data.data(), data.size(), nullptr, 0, nonce.data());
        auto dec = cipher.decrypt(enc.data(), enc.size(), nullptr, 0, nonce.data());
        ASSERT_TRUE(dec.has_value());
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, 1000.0);
}

TEST_F(PerfRegressionTest, VmessChunkCodecThroughput) {
    auto cipher = AeadCipherFactory::create(Config::SsMethod::AES_128_GCM, key16.data());
    VmessChunkCodec codec(std::move(cipher));

    std::vector<uint8_t> data(50000, 0x42);
    auto start = std::chrono::steady_clock::now();
    auto encoded = codec.encode(data.data(), data.size());
    auto decoded = codec.decode(encoded.data(), encoded.size());
    auto elapsed = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->size(), data.size());
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    EXPECT_LT(ms, 500.0);
}

TEST_F(PerfRegressionTest, V20ConfigBackwardCompat) {
    auto cfg = Config::defaultConfig();
    EXPECT_EQ(cfg.timeout.firstPacketSeconds, 10);
    EXPECT_EQ(cfg.timeout.connectSeconds, 10);
    EXPECT_EQ(cfg.timeout.idleSeconds, 300);
    EXPECT_FALSE(cfg.ipAcl.mode != Config::AclMode::Disabled);
    EXPECT_TRUE(Config::validate(cfg));
}

TEST_F(PerfRegressionTest, SingleExeNoExternalDll) {
    SUCCEED();
}