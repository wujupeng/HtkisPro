#include "Authenticator.h"
#include "UserManager.h"
#include "AuthFailureTracker.h"
#include "ConfigManager.h"
#include "Logger.h"

bool Authenticator::authenticate(const std::string& username,
                                  const std::string& password,
                                  const std::string& clientIp,
                                  const std::string& protocol) {
    if (AuthFailureTracker::getInstance().isBlocked(clientIp)) {
        Logger::getInstance().warn("[AUTH] Blocked IP " + clientIp + " attempted auth user=" + username);
        return false;
    }

    bool success = UserManager::getInstance().authenticate(username, password);

    if (success) {
        Logger::getInstance().info("[AUTH] Success user=" + username +
            " ip=" + clientIp + " protocol=" + protocol);
    } else {
        AuthFailureTracker::getInstance().recordFailure(clientIp);
        Logger::getInstance().warn("[AUTH] Failed user=" + username +
            " ip=" + clientIp + " protocol=" + protocol);
    }

    return success;
}

bool Authenticator::authenticateUuid(const std::string& uuid,
                                      const std::string& clientIp,
                                      const std::string& protocol) {
    if (AuthFailureTracker::getInstance().isBlocked(clientIp)) {
        Logger::getInstance().warn("[AUTH] Blocked IP " + clientIp + " attempted " + protocol + " auth");
        return false;
    }

    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    bool found = false;
    for (const auto& u : cfg->vmess.uuids) {
        if (u == uuid) { found = true; break; }
    }
    if (!found) {
        for (const auto& u : cfg->vless.uuids) {
            if (u == uuid) { found = true; break; }
        }
    }

    if (found) {
        Logger::getInstance().info("[AUTH] Success uuid-auth ip=" + clientIp + " protocol=" + protocol);
    } else {
        AuthFailureTracker::getInstance().recordFailure(clientIp);
        Logger::getInstance().warn("[AUTH] Failed uuid-auth ip=" + clientIp + " protocol=" + protocol);
    }

    return found;
}

bool Authenticator::authenticateTrojanHash(const std::string& hash,
                                            const std::string& clientIp) {
    if (AuthFailureTracker::getInstance().isBlocked(clientIp)) {
        Logger::getInstance().warn("[AUTH] Blocked IP " + clientIp + " attempted Trojan auth");
        return false;
    }

    auto cfg = Config::ConfigManager::getInstance().getSnapshot();
    bool found = false;
    for (const auto& h : cfg->trojan.passwordHashes) {
        if (h == hash) { found = true; break; }
    }

    if (found) {
        Logger::getInstance().info("[AUTH] Success trojan-auth ip=" + clientIp);
    } else {
        AuthFailureTracker::getInstance().recordFailure(clientIp);
        Logger::getInstance().warn("[AUTH] Failed trojan-auth ip=" + clientIp);
    }

    return found;
}

void Authenticator::recordShadowsocksFailure(const std::string& clientIp) {
    AuthFailureTracker::getInstance().recordFailure(clientIp);
    Logger::getInstance().warn("[AUTH] Shadowsocks decryption failed ip=" + clientIp);
}