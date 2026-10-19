#include "Authenticator.h"
#include "UserManager.h"
#include "AuthFailureTracker.h"
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