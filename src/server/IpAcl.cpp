#include "IpAcl.h"
#include "Logger.h"
#include <cstring>
#include <cstdlib>

bool IpAcl::ipToV4(const std::string& s, uint32_t& out) {
    int a, b, c, d;
    if (sscanf(s.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) return false;
    out = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
          (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
    return true;
}

bool IpAcl::ipToV6(const std::string& s, uint8_t out[16]) {
    memset(out, 0, 16);
    if (s == "::1" || s == "::") {
        if (s == "::1") out[15] = 1;
        return true;
    }

    int parts[8] = {0};
    int idx = 0;
    int doubleColonPos = -1;
    const char* p = s.c_str();

    while (*p && idx < 8) {
        if (*p == ':') {
            if (*(p + 1) == ':') {
                doubleColonPos = idx;
                p += 2;
                continue;
            }
            p++;
            continue;
        }
        char* end;
        long val = strtol(p, &end, 16);
        if (end == p || val < 0 || val > 0xFFFF) return false;
        parts[idx++] = static_cast<int>(val);
        p = end;
        if (*p == ':') p++;
    }

    if (doubleColonPos >= 0) {
        int fillCount = 8 - idx;
        for (int i = idx - 1; i >= doubleColonPos; i--) {
            parts[i + fillCount] = parts[i];
            parts[i] = 0;
        }
    }

    for (int i = 0; i < 8; i++) {
        out[i * 2] = static_cast<uint8_t>(parts[i] >> 8);
        out[i * 2 + 1] = static_cast<uint8_t>(parts[i] & 0xFF);
    }
    return true;
}

bool IpAcl::parseCidr(const std::string& cidr, ParsedRule& out) {
    out.original = cidr;
    auto slash = cidr.find('/');
    std::string ipStr = (slash != std::string::npos) ? cidr.substr(0, slash) : cidr;
    int prefix = (slash != std::string::npos) ? atoi(cidr.substr(slash + 1).c_str()) : -1;

    out.isIpv6 = ipStr.find(':') != std::string::npos;

    if (out.isIpv6) {
        if (prefix < 0) prefix = 128;
        if (prefix > 128) return false;
        out.prefixLen = static_cast<uint8_t>(prefix);
        if (!ipToV6(ipStr, out.v6Network)) return false;
        memset(out.v6Mask, 0, 16);
        for (int i = 0; i < prefix; i++) {
            out.v6Mask[i / 8] |= (0x80 >> (i % 8));
        }
        for (int i = 0; i < 16; i++) {
            out.v6Network[i] &= out.v6Mask[i];
        }
    } else {
        if (prefix < 0) prefix = 32;
        if (prefix > 32) return false;
        out.prefixLen = static_cast<uint8_t>(prefix);
        uint32_t ip;
        if (!ipToV4(ipStr, ip)) return false;
        out.v4Network = ip;
        out.v4Mask = (prefix == 0) ? 0 : (~static_cast<uint32_t>(0) << (32 - prefix));
        out.v4Network &= out.v4Mask;
    }
    return true;
}

bool IpAcl::matchV4(uint32_t ip, const ParsedRule& rule) {
    return (ip & rule.v4Mask) == rule.v4Network;
}

bool IpAcl::matchV6(const uint8_t ip[16], const ParsedRule& rule) {
    for (int i = 0; i < 16; i++) {
        if ((ip[i] & rule.v6Mask[i]) != rule.v6Network[i]) return false;
    }
    return true;
}

bool IpAcl::check(const std::string& ipStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_mode == Config::AclMode::Disabled) return true;

    bool isIpv6 = ipStr.find(':') != std::string::npos;
    const auto& rules = isIpv6 ? m_rulesV6 : m_rulesV4;

    bool matched = false;
    if (isIpv6) {
        uint8_t ip[16];
        if (!ipToV6(ipStr, ip)) return m_mode == Config::AclMode::Whitelist ? false : true;
        for (const auto& rule : rules) {
            if (matchV6(ip, rule)) { matched = true; break; }
        }
    } else {
        uint32_t ip;
        if (!ipToV4(ipStr, ip)) return m_mode == Config::AclMode::Whitelist ? false : true;
        for (const auto& rule : rules) {
            if (matchV4(ip, rule)) { matched = true; break; }
        }
    }

    if (m_mode == Config::AclMode::Whitelist) return matched;
    return !matched;
}

void IpAcl::refreshConfig(std::shared_ptr<const Config::IpAclConfig> cfg) {
    if (!cfg) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mode = cfg->mode;
    m_rulesV4.clear();
    m_rulesV6.clear();

    for (const auto& rule : cfg->rules) {
        ParsedRule pr;
        if (parseCidr(rule.network, pr)) {
            if (pr.isIpv6) m_rulesV6.push_back(pr);
            else m_rulesV4.push_back(pr);
        } else {
            Logger::getInstance().warn("[ACL] Invalid CIDR rule: " + rule.network);
        }
    }

    Logger::getInstance().info("[ACL] Refreshed: mode=" +
        Config::aclModeToString(m_mode) +
        " v4rules=" + std::to_string(m_rulesV4.size()) +
        " v6rules=" + std::to_string(m_rulesV6.size()));
}
