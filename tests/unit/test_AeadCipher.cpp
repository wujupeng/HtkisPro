#include <gtest/gtest.h>
#include "AeadCipher.h"
#include <cstring>

class AeadCipherTest : public ::testing::Test {
protected:
    void SetUp() override {
        key32.assign(32, 0);
        for (int i = 0; i < 32; ++i) key32[i] = static_cast<uint8_t>(i);
        key16.assign(16, 0);
        for (int i = 0; i < 16; ++i) key16[i] = static_cast<uint8_t>(i + 0x10);
        nonce.assign(12, 0);
        nonce[11] = 1;
    }
    std::vector<uint8_t> key32;
    std::vector<uint8_t> key16;
    std::vector<uint8_t> nonce;
};

TEST_F(AeadCipherTest, Aes256GcmRoundTrip) {
    Aes256GcmCipher cipher(key32.data());
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto ciphertext = cipher.encrypt(plaintext.data(), plaintext.size(), nullptr, 0, nonce.data());
    ASSERT_EQ(ciphertext.size(), plaintext.size() + cipher.tagLength());
    auto decrypted = cipher.decrypt(ciphertext.data(), ciphertext.size(), nullptr, 0, nonce.data());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(decrypted->size(), plaintext.size());
    EXPECT_EQ(std::memcmp(decrypted->data(), plaintext.data(), plaintext.size()), 0);
}

TEST_F(AeadCipherTest, Aes128GcmRoundTrip) {
    Aes128GcmCipher cipher(key16.data());
    std::vector<uint8_t> plaintext = {0xAA, 0xBB, 0xCC, 0xDD};
    auto ciphertext = cipher.encrypt(plaintext.data(), plaintext.size(), nullptr, 0, nonce.data());
    auto decrypted = cipher.decrypt(ciphertext.data(), ciphertext.size(), nullptr, 0, nonce.data());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(AeadCipherTest, ChaCha20Poly1305RoundTrip) {
    ChaCha20IetfPoly1305Cipher cipher(key32.data());
    std::vector<uint8_t> plaintext(100, 0x42);
    auto ciphertext = cipher.encrypt(plaintext.data(), plaintext.size(), nullptr, 0, nonce.data());
    auto decrypted = cipher.decrypt(ciphertext.data(), ciphertext.size(), nullptr, 0, nonce.data());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(AeadCipherTest, TagVerificationFailsOnTamper) {
    Aes256GcmCipher cipher(key32.data());
    std::vector<uint8_t> plaintext = {1, 2, 3};
    auto ciphertext = cipher.encrypt(plaintext.data(), plaintext.size(), nullptr, 0, nonce.data());
    ciphertext.back() ^= 0xFF;
    auto decrypted = cipher.decrypt(ciphertext.data(), ciphertext.size(), nullptr, 0, nonce.data());
    EXPECT_FALSE(decrypted.has_value());
}

TEST_F(AeadCipherTest, AADProcessing) {
    Aes256GcmCipher cipher(key32.data());
    std::vector<uint8_t> plaintext = {1, 2, 3};
    std::vector<uint8_t> aad = {0xDE, 0xAD};
    auto ciphertext = cipher.encrypt(plaintext.data(), plaintext.size(), aad.data(), aad.size(), nonce.data());
    auto decrypted = cipher.decrypt(ciphertext.data(), ciphertext.size(), aad.data(), aad.size(), nonce.data());
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(*decrypted, plaintext);
}

TEST_F(AeadCipherTest, FactoryCreatesCorrectCipher) {
    auto cipher = AeadCipherFactory::create(Config::SsMethod::AES_256_GCM, key32.data());
    EXPECT_EQ(cipher->keyLength(), 32);
    EXPECT_EQ(cipher->nonceLength(), 12);
    EXPECT_EQ(cipher->tagLength(), 16);
}