#include <gtest/gtest.h>
#include "SsKeyDerivation.h"
#include <cstring>

TEST(SsKeyDerivationTest, DeriveKeyLength16) {
    auto key = SsKeyDerivation::derive("test-password", 16);
    EXPECT_EQ(key.size(), 16);
}

TEST(SsKeyDerivationTest, DeriveKeyLength32) {
    auto key = SsKeyDerivation::derive("test-password", 32);
    EXPECT_EQ(key.size(), 32);
}

TEST(SsKeyDerivationTest, SamePasswordSameKey) {
    auto key1 = SsKeyDerivation::derive("same-pass", 32);
    auto key2 = SsKeyDerivation::derive("same-pass", 32);
    EXPECT_EQ(key1, key2);
}

TEST(SsKeyDerivationTest, DifferentPasswordDifferentKey) {
    auto key1 = SsKeyDerivation::derive("pass1", 32);
    auto key2 = SsKeyDerivation::derive("pass2", 32);
    EXPECT_NE(key1, key2);
}

TEST(SsKeyDerivationTest, EmptyPassword) {
    auto key = SsKeyDerivation::derive("", 32);
    EXPECT_EQ(key.size(), 32);
}