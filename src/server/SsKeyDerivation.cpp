#include "SsKeyDerivation.h"
#include <openssl/evp.h>
#include <cstring>

std::vector<uint8_t> SsKeyDerivation::derive(const std::string& password, std::size_t keyLength) {
    std::vector<uint8_t> result;
    result.reserve(keyLength);

    std::vector<uint8_t> prev;

    while (result.size() < keyLength) {
        EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(mdCtx, EVP_md5(), nullptr);

        if (!prev.empty()) {
            EVP_DigestUpdate(mdCtx, prev.data(), prev.size());
        }
        EVP_DigestUpdate(mdCtx, password.data(), password.size());

        unsigned char md5Result[16];
        unsigned int md5Len = 0;
        EVP_DigestFinal_ex(mdCtx, md5Result, &md5Len);
        EVP_MD_CTX_free(mdCtx);

        prev.assign(md5Result, md5Result + 16);
        size_t toAdd = (std::min)(static_cast<std::size_t>(16), keyLength - result.size());
        result.insert(result.end(), md5Result, md5Result + toAdd);
    }

    return result;
}