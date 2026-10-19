#include "HashUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#pragma comment(lib, "bcrypt.lib")

std::string HashUtil::sha256(const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashLen = 0;
    DWORD cbData = 0;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0) return {};

    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(DWORD), &cbData, 0);
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), 0);

    std::vector<uint8_t> hash(hashLen);
    BCryptFinishHash(hHash, hash.data(), hashLen, 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return toHex(hash);
}

std::string HashUtil::generateSalt(size_t length) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0);

    std::vector<uint8_t> bytes(length);
    BCryptGenRandom(hAlg, bytes.data(), static_cast<ULONG>(length), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return toHex(bytes);
}

#else
#include <openssl/sha.h>
#include <openssl/rand.h>

std::string HashUtil::sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return toHex(std::vector<uint8_t>(hash, hash + SHA256_DIGEST_LENGTH));
}

std::string HashUtil::generateSalt(size_t length) {
    std::vector<uint8_t> bytes(length);
    RAND_bytes(bytes.data(), static_cast<int>(length));
    return toHex(bytes);
}
#endif

std::string HashUtil::sha256Hex(const std::string& salt, const std::string& password) {
    return sha256(salt + password);
}

std::string HashUtil::toHex(const std::vector<uint8_t>& bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    static const char digits[] = "0123456789abcdef";
    for (auto b : bytes) {
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0xF]);
    }
    return hex;
}