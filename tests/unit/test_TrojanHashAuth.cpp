#include <gtest/gtest.h>
#include "TrojanHashAuth.h"

TEST(TrojanHashAuthTest, ComputeHashLength) {
    auto hash = TrojanHashAuth::computeHash("password123");
    EXPECT_EQ(hash.size(), 56);
}

TEST(TrojanHashAuthTest, SamePasswordSameHash) {
    auto h1 = TrojanHashAuth::computeHash("test");
    auto h2 = TrojanHashAuth::computeHash("test");
    EXPECT_EQ(h1, h2);
}

TEST(TrojanHashAuthTest, DifferentPasswordDifferentHash) {
    auto h1 = TrojanHashAuth::computeHash("pass1");
    auto h2 = TrojanHashAuth::computeHash("pass2");
    EXPECT_NE(h1, h2);
}

TEST(TrojanHashAuthTest, ValidateCorrectHash) {
    auto hash = TrojanHashAuth::computeHash("mypassword");
    std::vector<std::string> hashes = {hash};
    EXPECT_TRUE(TrojanHashAuth::validate(hash, hashes));
}

TEST(TrojanHashAuthTest, ValidateIncorrectHash) {
    std::vector<std::string> hashes = {TrojanHashAuth::computeHash("correct")};
    EXPECT_FALSE(TrojanHashAuth::validate("wronghash", hashes));
}

TEST(TrojanHashAuthTest, MultiplePasswordSupport) {
    auto h1 = TrojanHashAuth::computeHash("pass1");
    auto h2 = TrojanHashAuth::computeHash("pass2");
    std::vector<std::string> hashes = {h1, h2};
    EXPECT_TRUE(TrojanHashAuth::validate(h1, hashes));
    EXPECT_TRUE(TrojanHashAuth::validate(h2, hashes));
    EXPECT_FALSE(TrojanHashAuth::validate("unknown", hashes));
}