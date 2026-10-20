#include "AesCfbCipher.h"
#include <openssl/evp.h>
#include <cstring>

std::vector<uint8_t> AesCfbCipher::encrypt(const uint8_t* plaintext, std::size_t length,
                                            const uint8_t* key, std::size_t keyLength,
                                            const uint8_t* iv, std::size_t ivLength) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    const EVP_CIPHER* cipher = (keyLength == 16) ? EVP_aes_128_cfb() : EVP_aes_256_cfb();

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    std::vector<uint8_t> output(length);
    int outLen = 0;

    if (EVP_EncryptUpdate(ctx, output.data(), &outLen, plaintext, static_cast<int>(length)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int totalLen = outLen;
    if (EVP_EncryptFinal_ex(ctx, output.data() + totalLen, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += outLen;

    EVP_CIPHER_CTX_free(ctx);
    output.resize(totalLen);
    return output;
}

std::vector<uint8_t> AesCfbCipher::decrypt(const uint8_t* ciphertext, std::size_t length,
                                            const uint8_t* key, std::size_t keyLength,
                                            const uint8_t* iv, std::size_t ivLength) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    const EVP_CIPHER* cipher = (keyLength == 16) ? EVP_aes_128_cfb() : EVP_aes_256_cfb();

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    std::vector<uint8_t> output(length);
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx, output.data(), &outLen, ciphertext, static_cast<int>(length)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int totalLen = outLen;
    if (EVP_DecryptFinal_ex(ctx, output.data() + totalLen, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += outLen;

    EVP_CIPHER_CTX_free(ctx);
    output.resize(totalLen);
    return output;
}

static std::vector<uint8_t> md5(const uint8_t* data, std::size_t length) {
    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdCtx, EVP_md5(), nullptr);
    EVP_DigestUpdate(mdCtx, data, length);
    unsigned char result[16];
    unsigned int resultLen = 0;
    EVP_DigestFinal_ex(mdCtx, result, &resultLen);
    EVP_MD_CTX_free(mdCtx);
    return std::vector<uint8_t>(result, result + 16);
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> AesCfbCipher::deriveVmessKey(const std::string& uuid) {
    std::vector<uint8_t> uuidBytes(uuid.begin(), uuid.end());

    auto key = md5(uuidBytes.data(), uuidBytes.size());

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), key.begin(), key.end());
    combined.insert(combined.end(), uuidBytes.begin(), uuidBytes.end());
    auto iv = md5(combined.data(), combined.size());

    return {key, iv};
}