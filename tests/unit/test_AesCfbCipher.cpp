#include <gtest/gtest.h>
#include "AesCfbCipher.h"
#include <cstring>

TEST(AesCfbCipherTest, EncryptDecryptRoundTrip) {
    std::vector<uint8_t> key(16, 0x42);
    std::vector<uint8_t> iv(16, 0x33);
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    auto ciphertext = AesCfbCipher::encrypt(plaintext.data(), plaintext.size(), key.data(), key.size(), iv.data(), iv.size());
    auto decrypted = AesCfbCipher::decrypt(ciphertext.data(), ciphertext.size(), key.data(), key.size(), iv.data(), iv.size());
    EXPECT_EQ(decrypted, plaintext);
}

TEST(AesCfbCipherTest, DeriveVmessKeyProducesValidKeys) {
    auto [key, iv] = AesCfbCipher::deriveVmessKey("12345678-1234-1234-1234-123456789abc");
    EXPECT_EQ(key.size(), 16);
    EXPECT_EQ(iv.size(), 16);
}

TEST(AesCfbCipherTest, DeriveVmessKeyConsistent) {
    auto [key1, iv1] = AesCfbCipher::deriveVmessKey("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    auto [key2, iv2] = AesCfbCipher::deriveVmessKey("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    EXPECT_EQ(key1, key2);
    EXPECT_EQ(iv1, iv2);
}

TEST(AesCfbCipherTest, DifferentUuidDifferentKey) {
    auto [key1, iv1] = AesCfbCipher::deriveVmessKey("11111111-1111-1111-1111-111111111111");
    auto [key2, iv2] = AesCfbCipher::deriveVmessKey("22222222-2222-2222-2222-222222222222");
    EXPECT_NE(key1, key2);
}