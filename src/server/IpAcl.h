#pragma once

#include "Config.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>

class IpAcl {
public:
    static IpAcl& getInstance() {
        static IpAcl instance;
        return instance;
    }

    bool check(const std::string& ip) const;

    void refreshConfig(std::shared_ptr<const Config::IpAclConfig> cfg);

private:
    IpAcl() = default;

    struct ParsedRule {
        std::string original;
        bool isIpv6;
        uint32_t v4Network;
        uint32_t v4Mask;
        uint8_t v6Network[16];
        uint8_t v6Mask[16];
        uint8_t prefixLen;
    };

    static bool parseCidr(const std::string& cidr, ParsedRule& out);
    static bool matchV4(uint32_t ip, const ParsedRule& rule);
    static bool matchV6(const uint8_t ip[16], const ParsedRule& rule);
    static bool ipToV4(const std::string& s, uint32_t& out);
    static bool ipToV6(const std::string& s, uint8_t out[16]);

    mutable std::mutex m_mutex;
    Config::AclMode m_mode{Config::AclMode::Disabled};
    std::vector<ParsedRule> m_rulesV4;
    std::vector<ParsedRule> m_rulesV6;
};
