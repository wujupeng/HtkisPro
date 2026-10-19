#include <gtest/gtest.h>
#include "HashUtil.h"
#include <string>

TEST(HashUtilTest, Sha256KnownValue) {
    std::string hash = HashUtil::sha256("hello");
    EXPECT_EQ(hash.length(), 64u);
    std::string expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    EXPECT_EQ(hash, expected);
}

TEST(HashUtilTest, Sha256EmptyString) {
    std::string hash = HashUtil::sha256("");
    EXPECT_EQ(hash.length(), 64u);
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(hash, expected);
}

TEST(HashUtilTest, Sha256HexConsistency) {
    std::string salt = "abc123";
    std::string password = "mypassword";
    std::string hash1 = HashUtil::sha256Hex(salt, password);
    std::string hash2 = HashUtil::sha256Hex(salt, password);
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.length(), 64u);
}

TEST(HashUtilTest, Sha256HexDifferentSalt) {
    std::string password = "mypassword";
    std::string hash1 = HashUtil::sha256Hex("salt1", password);
    std::string hash2 = HashUtil::sha256Hex("salt2", password);
    EXPECT_NE(hash1, hash2);
}

TEST(HashUtilTest, Sha256HexDifferentPassword) {
    std::string salt = "fixedsalt";
    std::string hash1 = HashUtil::sha256Hex(salt, "pass1");
    std::string hash2 = HashUtil::sha256Hex(salt, "pass2");
    EXPECT_NE(hash1, hash2);
}

TEST(HashUtilTest, GenerateSaltLength) {
    std::string salt16 = HashUtil::generateSalt(16);
    EXPECT_EQ(salt16.length(), 32u);
    std::string salt32 = HashUtil::generateSalt(32);
    EXPECT_EQ(salt32.length(), 64u);
}

TEST(HashUtilTest, GenerateSaltRandomness) {
    std::string salt1 = HashUtil::generateSalt(16);
    std::string salt2 = HashUtil::generateSalt(16);
    EXPECT_NE(salt1, salt2);
}

TEST(HashUtilTest, ToHexConversion) {
    std::vector<uint8_t> bytes = {0x00, 0x0f, 0xff, 0xab, 0xcd};
    std::string hex = HashUtil::toHex(bytes);
    EXPECT_EQ(hex, "000fffabcd");
}

TEST(HashUtilTest, ToHexEmpty) {
    std::vector<uint8_t> empty;
    std::string hex = HashUtil::toHex(empty);
    EXPECT_EQ(hex, "");
}