#include "TrojanHashAuth.h"
#include <openssl/sha.h>
#include <cstring>

static char toHexChar(uint8_t v) {
    return v < 10 ? static_cast<char>('0' + v) : static_cast<char>('a' + v - 10);
}

std::string TrojanHashAuth::computeHash(const std::string& password) {
    unsigned char hash[SHA224_DIGEST_LENGTH];
    SHA224(reinterpret_cast<const unsigned char*>(password.data()), password.size(), hash);

    std::string hex;
    hex.reserve(SHA224_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA224_DIGEST_LENGTH; ++i) {
        hex.push_back(toHexChar(hash[i] >> 4));
        hex.push_back(toHexChar(hash[i] & 0x0F));
    }
    return hex;
}

bool TrojanHashAuth::validate(const std::string& clientHash, const std::vector<std::string>& passwordHashes) {
    for (const auto& h : passwordHashes) {
        if (h == clientHash) return true;
    }
    return false;
}