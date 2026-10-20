#include <gtest/gtest.h>
#include "AeadCipher.h"
#include "AesCfbCipher.h"
#include "SsKeyDerivation.h"
#include "VmessHeaderDecoder.h"
#include "VlessHeaderParser.h"
#include "TrojanHashAuth.h"
#include "Config.h"

class CryptoProtocolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        key32.assign(32, 0x42);
        nonce.assign(12, 0);
    }
    std::vector<uint8_t> key32;
    std::vector<uint8_t> nonce;
};

TEST_F(CryptoProtocolIntegrationTest, ShadowsocksAeadFlow) {
    auto masterKey = SsKeyDerivation::derive("test-pass", 32);
    ASSERT_EQ(masterKey.size(), 32);

    Aes256GcmCipher cipher(masterKey.data());
    std::vector<uint8_t> payload = {0x01, 127, 0, 0, 1, 0x00, 0x50};

    auto encrypted = cipher.encrypt(payload.data(), payload.size(), nullptr, 0, nonce.data());
    auto decrypted = cipher.decrypt(encrypted.data(), encrypted.size(), nullptr, 0, nonce.data());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, payload);
}

TEST_F(CryptoProtocolIntegrationTest, VmessHeaderEncryptDecryptFlow) {
    std::string uuid = "12345678-1234-1234-1234-123456789abc";
    auto [key, iv] = AesCfbCipher::deriveVmessKey(uuid);

    std::vector<uint8_t> header;
    header.push_back(0x01);
    for (int i = 0; i < 16; ++i) header.push_back(0xAA);
    header.push_back(0);
    header.push_back(0x01);
    header.push_back(0x00); header.push_back(0x50);
    header.push_back(0x01);
    header.push_back(127); header.push_back(0); header.push_back(0); header.push_back(1);
    header.push_back(0); header.push_back(0); header.push_back(0); header.push_back(0);

    auto encrypted = AesCfbCipher::encrypt(header.data(), header.size(), key.data(), key.size(), iv.data(), iv.size());
    auto decrypted = AesCfbCipher::decrypt(encrypted.data(), encrypted.size(), key.data(), key.size(), iv.data(), iv.size());
    EXPECT_EQ(decrypted, header);

    auto parsed = VmessHeaderDecoder::decode(decrypted.data(), decrypted.size());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->host, "127.0.0.1");
    EXPECT_EQ(parsed->port, 80);
}

TEST_F(CryptoProtocolIntegrationTest, VlessPlaintextFlow) {
    std::vector<uint8_t> header;
    header.push_back(0x00);
    for (int i = 0; i < 16; ++i) header.push_back(0xBB);
    header.push_back(0);
    header.push_back(0x01);
    header.push_back(0x01); header.push_back(0xBB);
    header.push_back(0x02);
    const char* domain = "example.com";
    header.push_back(static_cast<uint8_t>(strlen(domain)));
    for (size_t i = 0; i < strlen(domain); ++i) header.push_back(domain[i]);

    auto parsed = VlessHeaderParser::parse(header.data(), header.size());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->host, "example.com");
    EXPECT_EQ(parsed->port, 443);
}

TEST_F(CryptoProtocolIntegrationTest, TrojanHashFlow) {
    std::string password = "trojan-pass";
    auto hash = TrojanHashAuth::computeHash(password);
    EXPECT_EQ(hash.size(), 56);

    std::vector<std::string> hashes = {hash};
    EXPECT_TRUE(TrojanHashAuth::validate(hash, hashes));

    std::string fakeHash(56, '0');
    EXPECT_FALSE(TrojanHashAuth::validate(fakeHash, hashes));
}

TEST_F(CryptoProtocolIntegrationTest, MultiProtocolCoexistence) {
    auto cfg = Config::defaultConfig();
    cfg.shadowsocks.enabled = true;
    cfg.vmess.enabled = true;
    cfg.vless.enabled = true;
    cfg.trojan.enabled = false;

    Config::SsPortConfig ssPort;
    ssPort.port = 8388;
    ssPort.method = Config::SsMethod::AES_256_GCM;
    cfg.shadowsocks.ports.push_back(ssPort);

    cfg.vmess.port = 10086;
    cfg.vmess.uuids.push_back("12345678-1234-1234-1234-123456789abc");
    cfg.vless.port = 10087;
    cfg.vless.uuids.push_back("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

    EXPECT_NE(cfg.shadowsocks.ports[0].port, cfg.vmess.port);
    EXPECT_NE(cfg.vmess.port, cfg.vless.port);
    EXPECT_NE(cfg.shadowsocks.ports[0].port, cfg.vless.port);
    EXPECT_TRUE(Config::validate(cfg));
}

TEST_F(CryptoProtocolIntegrationTest, V20ProtocolNoRegression) {
    auto cfg = Config::defaultConfig();
    EXPECT_FALSE(cfg.shadowsocks.enabled);
    EXPECT_FALSE(cfg.vmess.enabled);
    EXPECT_FALSE(cfg.vless.enabled);
    EXPECT_FALSE(cfg.trojan.enabled);
    EXPECT_TRUE(Config::validate(cfg));
}