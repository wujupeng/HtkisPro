#pragma once

#include <string>
#include <vector>

class HashUtil {
public:
    static std::string sha256(const std::string& input);
    static std::string sha256Hex(const std::string& salt, const std::string& password);
    static std::string generateSalt(size_t length = 16);
    static std::string toHex(const std::vector<uint8_t>& bytes);
};