#include "AeadCipher.h"
#include <openssl/evp.h>
#include <cstring>

static std::vector<uint8_t> aeadEncrypt(const EVP_CIPHER* cipher, const uint8_t* key,
                                         const uint8_t* plaintext, std::size_t length,
                                         const uint8_t* aad, std::size_t aadLength,
                                         const uint8_t* nonce) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> output(length + 16);

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int outLen = 0;
    if (aadLength > 0) {
        if (EVP_EncryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLength)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
    }

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

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output.data() + totalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += 16;

    EVP_CIPHER_CTX_free(ctx);
    output.resize(totalLen);
    return output;
}

static std::optional<std::vector<uint8_t>> aeadDecrypt(const EVP_CIPHER* cipher, const uint8_t* key,
                                                        const uint8_t* ciphertext, std::size_t length,
                                                        const uint8_t* aad, std::size_t aadLength,
                                                        const uint8_t* nonce) {
    if (length < 16) return std::nullopt;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return std::nullopt;

    std::vector<uint8_t> output(length - 16);

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }

    int outLen = 0;
    if (aadLength > 0) {
        if (EVP_DecryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLength)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::nullopt;
        }
    }

    if (EVP_DecryptUpdate(ctx, output.data(), &outLen, ciphertext, static_cast<int>(length - 16)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }

    int totalLen = outLen;

    uint8_t tag[16];
    std::memcpy(tag, ciphertext + length - 16, 16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }

    if (EVP_DecryptFinal_ex(ctx, output.data() + totalLen, &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::nullopt;
    }
    totalLen += outLen;

    EVP_CIPHER_CTX_free(ctx);
    output.resize(totalLen);
    return output;
}

Aes256GcmCipher::Aes256GcmCipher(const uint8_t* key) {
    std::memcpy(m_key, key, 32);
}

std::vector<uint8_t> Aes256GcmCipher::encrypt(const uint8_t* plaintext, std::size_t length,
                                               const uint8_t* aad, std::size_t aadLength,
                                               const uint8_t* nonce) {
    return aeadEncrypt(EVP_aes_256_gcm(), m_key, plaintext, length, aad, aadLength, nonce);
}

std::optional<std::vector<uint8_t>> Aes256GcmCipher::decrypt(const uint8_t* ciphertext, std::size_t length,
                                                              const uint8_t* aad, std::size_t aadLength,
                                                              const uint8_t* nonce) {
    return aeadDecrypt(EVP_aes_256_gcm(), m_key, ciphertext, length, aad, aadLength, nonce);
}

Aes128GcmCipher::Aes128GcmCipher(const uint8_t* key) {
    std::memcpy(m_key, key, 16);
}

std::vector<uint8_t> Aes128GcmCipher::encrypt(const uint8_t* plaintext, std::size_t length,
                                               const uint8_t* aad, std::size_t aadLength,
                                               const uint8_t* nonce) {
    return aeadEncrypt(EVP_aes_128_gcm(), m_key, plaintext, length, aad, aadLength, nonce);
}

std::optional<std::vector<uint8_t>> Aes128GcmCipher::decrypt(const uint8_t* ciphertext, std::size_t length,
                                                              const uint8_t* aad, std::size_t aadLength,
                                                              const uint8_t* nonce) {
    return aeadDecrypt(EVP_aes_128_gcm(), m_key, ciphertext, length, aad, aadLength, nonce);
}

ChaCha20IetfPoly1305Cipher::ChaCha20IetfPoly1305Cipher(const uint8_t* key) {
    std::memcpy(m_key, key, 32);
}

std::vector<uint8_t> ChaCha20IetfPoly1305Cipher::encrypt(const uint8_t* plaintext, std::size_t length,
                                                          const uint8_t* aad, std::size_t aadLength,
                                                          const uint8_t* nonce) {
    return aeadEncrypt(EVP_chacha20_poly1305(), m_key, plaintext, length, aad, aadLength, nonce);
}

std::optional<std::vector<uint8_t>> ChaCha20IetfPoly1305Cipher::decrypt(const uint8_t* ciphertext, std::size_t length,
                                                                         const uint8_t* aad, std::size_t aadLength,
                                                                         const uint8_t* nonce) {
    return aeadDecrypt(EVP_chacha20_poly1305(), m_key, ciphertext, length, aad, aadLength, nonce);
}

std::unique_ptr<AeadCipher> AeadCipherFactory::create(Config::SsMethod method, const uint8_t* key) {
    switch (method) {
        case Config::SsMethod::AES_256_GCM:
            return std::make_unique<Aes256GcmCipher>(key);
        case Config::SsMethod::AES_128_GCM:
            return std::make_unique<Aes128GcmCipher>(key);
        case Config::SsMethod::CHACHA20_IETF_POLY1305:
            return std::make_unique<ChaCha20IetfPoly1305Cipher>(key);
        default:
            return nullptr;
    }
}