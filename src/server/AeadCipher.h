#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include "Config.h"

class AeadCipher {
public:
    virtual ~AeadCipher() = default;

    virtual std::vector<uint8_t> encrypt(const uint8_t* plaintext, std::size_t length,
                                          const uint8_t* aad, std::size_t aadLength,
                                          const uint8_t* nonce) = 0;
    virtual std::optional<std::vector<uint8_t>> decrypt(const uint8_t* ciphertext, std::size_t length,
                                                         const uint8_t* aad, std::size_t aadLength,
                                                         const uint8_t* nonce) = 0;
    virtual std::size_t keyLength() const = 0;
    virtual std::size_t nonceLength() const = 0;
    virtual std::size_t tagLength() const = 0;
};

class Aes256GcmCipher : public AeadCipher {
public:
    explicit Aes256GcmCipher(const uint8_t* key);
    std::vector<uint8_t> encrypt(const uint8_t* plaintext, std::size_t length,
                                  const uint8_t* aad, std::size_t aadLength,
                                  const uint8_t* nonce) override;
    std::optional<std::vector<uint8_t>> decrypt(const uint8_t* ciphertext, std::size_t length,
                                                 const uint8_t* aad, std::size_t aadLength,
                                                 const uint8_t* nonce) override;
    std::size_t keyLength() const override { return 32; }
    std::size_t nonceLength() const override { return 12; }
    std::size_t tagLength() const override { return 16; }
private:
    uint8_t m_key[32];
};

class Aes128GcmCipher : public AeadCipher {
public:
    explicit Aes128GcmCipher(const uint8_t* key);
    std::vector<uint8_t> encrypt(const uint8_t* plaintext, std::size_t length,
                                  const uint8_t* aad, std::size_t aadLength,
                                  const uint8_t* nonce) override;
    std::optional<std::vector<uint8_t>> decrypt(const uint8_t* ciphertext, std::size_t length,
                                                 const uint8_t* aad, std::size_t aadLength,
                                                 const uint8_t* nonce) override;
    std::size_t keyLength() const override { return 16; }
    std::size_t nonceLength() const override { return 12; }
    std::size_t tagLength() const override { return 16; }
private:
    uint8_t m_key[16];
};

class ChaCha20IetfPoly1305Cipher : public AeadCipher {
public:
    explicit ChaCha20IetfPoly1305Cipher(const uint8_t* key);
    std::vector<uint8_t> encrypt(const uint8_t* plaintext, std::size_t length,
                                  const uint8_t* aad, std::size_t aadLength,
                                  const uint8_t* nonce) override;
    std::optional<std::vector<uint8_t>> decrypt(const uint8_t* ciphertext, std::size_t length,
                                                 const uint8_t* aad, std::size_t aadLength,
                                                 const uint8_t* nonce) override;
    std::size_t keyLength() const override { return 32; }
    std::size_t nonceLength() const override { return 12; }
    std::size_t tagLength() const override { return 16; }
private:
    uint8_t m_key[32];
};

class AeadCipherFactory {
public:
    static std::unique_ptr<AeadCipher> create(Config::SsMethod method, const uint8_t* key);
};