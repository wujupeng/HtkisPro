#include <gtest/gtest.h>
#include "IpAcl.h"
#include "Config.h"
#include <memory>

static void setupAcl(Config::AclMode mode, std::vector<std::string> rules) {
    Config::IpAclConfig cfg;
    cfg.mode = mode;
    for (auto& r : rules) {
        Config::CidrRule rule;
        rule.network = r;
        rule.isIpv6 = (r.find(':') != std::string::npos);
        cfg.rules.push_back(rule);
    }
    IpAcl::getInstance().refreshConfig(
        std::make_shared<const Config::IpAclConfig>(cfg));
}

TEST(IpAclTest, DisabledAllowsAll) {
    setupAcl(Config::AclMode::Disabled, {});
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.1"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.0.1"));
    EXPECT_TRUE(IpAcl::getInstance().check("8.8.8.8"));
}

TEST(IpAclTest, WhitelistSingleIp) {
    setupAcl(Config::AclMode::Whitelist, {"192.168.1.100/32"});
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.100"));
    EXPECT_FALSE(IpAcl::getInstance().check("192.168.1.101"));
    EXPECT_FALSE(IpAcl::getInstance().check("10.0.0.1"));
}

TEST(IpAclTest, WhitelistCidr24) {
    setupAcl(Config::AclMode::Whitelist, {"192.168.1.0/24"});
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.0"));
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.255"));
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.128"));
    EXPECT_FALSE(IpAcl::getInstance().check("192.168.2.1"));
    EXPECT_FALSE(IpAcl::getInstance().check("10.0.0.1"));
}

TEST(IpAclTest, WhitelistCidr16) {
    setupAcl(Config::AclMode::Whitelist, {"10.0.0.0/16"});
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.0.1"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.255.254"));
    EXPECT_FALSE(IpAcl::getInstance().check("10.1.0.1"));
    EXPECT_FALSE(IpAcl::getInstance().check("11.0.0.1"));
}

TEST(IpAclTest, BlacklistSingleIp) {
    setupAcl(Config::AclMode::Blacklist, {"192.168.1.100/32"});
    EXPECT_FALSE(IpAcl::getInstance().check("192.168.1.100"));
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.101"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.0.1"));
}

TEST(IpAclTest, BlacklistCidr24) {
    setupAcl(Config::AclMode::Blacklist, {"10.0.0.0/24"});
    EXPECT_FALSE(IpAcl::getInstance().check("10.0.0.1"));
    EXPECT_FALSE(IpAcl::getInstance().check("10.0.0.255"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.1.1"));
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.1"));
}

TEST(IpAclTest, WhitelistMultipleRules) {
    setupAcl(Config::AclMode::Whitelist, {"192.168.1.0/24", "10.0.0.0/8"});
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.50"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.255.255.1"));
    EXPECT_FALSE(IpAcl::getInstance().check("172.16.0.1"));
}

TEST(IpAclTest, BlacklistMultipleRules) {
    setupAcl(Config::AclMode::Blacklist, {"10.0.0.0/8", "192.168.0.0/16"});
    EXPECT_FALSE(IpAcl::getInstance().check("10.1.2.3"));
    EXPECT_FALSE(IpAcl::getInstance().check("192.168.5.5"));
    EXPECT_TRUE(IpAcl::getInstance().check("8.8.8.8"));
    EXPECT_TRUE(IpAcl::getInstance().check("172.16.0.1"));
}

TEST(IpAclTest, WhitelistIpv6) {
    setupAcl(Config::AclMode::Whitelist, {"::1/128"});
    EXPECT_TRUE(IpAcl::getInstance().check("::1"));
    EXPECT_FALSE(IpAcl::getInstance().check("::2"));
}

TEST(IpAclTest, WhitelistIpv6Cidr) {
    setupAcl(Config::AclMode::Whitelist, {"2001:db8::/32"});
    EXPECT_TRUE(IpAcl::getInstance().check("2001:db8::1"));
    EXPECT_TRUE(IpAcl::getInstance().check("2001:db8:ffff:ffff:ffff:ffff:ffff:ffff"));
    EXPECT_FALSE(IpAcl::getInstance().check("2001:db9::1"));
}

TEST(IpAclTest, EmptyWhitelistDeniesAll) {
    setupAcl(Config::AclMode::Whitelist, {});
    EXPECT_FALSE(IpAcl::getInstance().check("192.168.1.1"));
    EXPECT_FALSE(IpAcl::getInstance().check("10.0.0.1"));
}

TEST(IpAclTest, EmptyBlacklistAllowsAll) {
    setupAcl(Config::AclMode::Blacklist, {});
    EXPECT_TRUE(IpAcl::getInstance().check("192.168.1.1"));
    EXPECT_TRUE(IpAcl::getInstance().check("10.0.0.1"));
}