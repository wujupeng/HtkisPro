#include <gtest/gtest.h>
#include "Config.h"
#include "ConfigManager.h"

TEST(CryptoConfigTest, DefaultConfigHasCryptoDisabled) {
    auto cfg = Config::defaultConfig();
    EXPECT_FALSE(cfg.shadowsocks.enabled);
    EXPECT_FALSE(cfg.vmess.enabled);
    EXPECT_FALSE(cfg.vless.enabled);
    EXPECT_FALSE(cfg.trojan.enabled);
    EXPECT_FALSE(cfg.nat.enabled);
}

TEST(CryptoConfigTest, DefaultShadowsocksConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_TRUE(cfg.shadowsocks.ports.empty());
}

TEST(CryptoConfigTest, DefaultVmessConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_EQ(cfg.vmess.port, 10086);
    EXPECT_EQ(cfg.vmess.alterId, 0);
    EXPECT_TRUE(cfg.vmess.uuids.empty());
}

TEST(CryptoConfigTest, DefaultVlessConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_EQ(cfg.vless.port, 10087);
    EXPECT_EQ(cfg.vless.transport, Config::VlessTransport::TCP);
}

TEST(CryptoConfigTest, DefaultTrojanConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_EQ(cfg.trojan.port, 443);
    EXPECT_TRUE(cfg.trojan.passwordHashes.empty());
}

TEST(CryptoConfigTest, DefaultNatConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_EQ(cfg.nat.stunPort, 3478);
    EXPECT_EQ(cfg.nat.keepaliveSeconds, 20);
    EXPECT_EQ(cfg.nat.natType, Config::NatType::UNKNOWN);
}

TEST(CryptoConfigTest, IsValidUuid) {
    EXPECT_TRUE(Config::isValidUuid("12345678-1234-1234-1234-123456789abc"));
    EXPECT_TRUE(Config::isValidUuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    EXPECT_FALSE(Config::isValidUuid("invalid"));
    EXPECT_FALSE(Config::isValidUuid("12345678-1234-1234-1234-123456789ab"));
    EXPECT_FALSE(Config::isValidUuid(""));
}

TEST(CryptoConfigTest, SsMethodToString) {
    EXPECT_EQ(Config::ssMethodToString(Config::SsMethod::AES_256_GCM), "aes-256-gcm");
    EXPECT_EQ(Config::ssMethodToString(Config::SsMethod::AES_128_GCM), "aes-128-gcm");
    EXPECT_EQ(Config::ssMethodToString(Config::SsMethod::CHACHA20_IETF_POLY1305), "chacha20-ietf-poly1305");
}

TEST(CryptoConfigTest, VlessTransportToString) {
    EXPECT_EQ(Config::vlessTransportToString(Config::VlessTransport::TCP), "tcp");
    EXPECT_EQ(Config::vlessTransportToString(Config::VlessTransport::TLS), "tls");
    EXPECT_EQ(Config::vlessTransportToString(Config::VlessTransport::WEBSOCKET), "websocket");
}

TEST(CryptoConfigTest, NatTypeToString) {
    EXPECT_EQ(Config::natTypeToString(Config::NatType::UNKNOWN), "unknown");
    EXPECT_EQ(Config::natTypeToString(Config::NatType::FULL_CONE), "full_cone");
    EXPECT_EQ(Config::natTypeToString(Config::NatType::SYMMETRIC), "symmetric");
}

TEST(CryptoConfigTest, ValidateDefaultConfig) {
    auto cfg = Config::defaultConfig();
    EXPECT_TRUE(Config::validate(cfg));
}