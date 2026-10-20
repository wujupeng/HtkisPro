#pragma once

#include <string>
#include <vector>

class TrojanHashAuth {
public:
    static std::string computeHash(const std::string& password);
    static bool validate(const std::string& clientHash, const std::vector<std::string>& passwordHashes);
};