#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <utility>

class AesCfbCipher {
public:
    static std::vector<uint8_t> encrypt(const uint8_t* plaintext, std::size_t length,
                                         const uint8_t* key, std::size_t keyLength,
                                         const uint8_t* iv, std::size_t ivLength);
    static std::vector<uint8_t> decrypt(const uint8_t* ciphertext, std::size_t length,
                                         const uint8_t* key, std::size_t keyLength,
                                         const uint8_t* iv, std::size_t ivLength);
    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> deriveVmessKey(const std::string& uuid);
};