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

private:
    Authenticator() = default;
};