#pragma once

#include <cstdint>
#include <vector>
#include <string>

class SsKeyDerivation {
public:
    static std::vector<uint8_t> derive(const std::string& password, std::size_t keyLength);
};