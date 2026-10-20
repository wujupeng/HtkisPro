#pragma once

#include <string>

class Authenticator {
public:
    static Authenticator& getInstance() {
        static Authenticator instance;
        return instance;
    }

    bool authenticate(const std::string& username,
                      const std::string& password,
                      const std::string& clientIp,
                      const std::string& protocol);

    bool authenticateUuid(const std::string& uuid,
                          const std::string& clientIp,
                          const std::string& protocol);

    bool authenticateTrojanHash(const std::string& hash,
                                const std::string& clientIp);

    void recordShadowsocksFailure(const std::string& clientIp);

private:
    Authenticator() = default;
};