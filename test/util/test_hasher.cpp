#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "util/Sha1Hasher.hpp"
#include "util/Sha256Hasher.hpp"
#include "util/IHasher.hpp"

using namespace gitter;

// Test: SHA-1 known test vectors
TEST(HasherTest, Sha1KnownVectors) {
    Sha1Hasher hasher;
    
    // Empty string
    hasher.update("");
    auto digest = hasher.digest();
    auto hex = IHasher::toHex(digest);
    EXPECT_EQ(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    
    // "abc"
    hasher.reset();
    hasher.update("abc");
    hex = IHasher::toHex(hasher.digest());
    EXPECT_EQ(hex, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

// Test: SHA-1 digest size
TEST(HasherTest, Sha1DigestSize) {
    Sha1Hasher hasher;
    EXPECT_EQ(hasher.digestSize(), 20);
    EXPECT_STREQ(hasher.name(), "sha1");
}

// Test: SHA-256 known test vectors
TEST(HasherTest, Sha256KnownVectors) {
    Sha256Hasher hasher;
    
    // Empty string
    hasher.update("");
    auto digest = hasher.digest();
    auto hex = IHasher::toHex(digest);
    EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    
    // "abc"
    hasher.reset();
    hasher.update("abc");
    hex = IHasher::toHex(hasher.digest());
    EXPECT_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// Test: SHA-256 digest size
TEST(HasherTest, Sha256DigestSize) {
    Sha256Hasher hasher;
    EXPECT_EQ(hasher.digestSize(), 32);
    EXPECT_STREQ(hasher.name(), "sha256");
}

// Test: Hasher factory creates SHA-1 by default
TEST(HasherTest, FactoryCreatesDefault) {
    auto hasher = HasherFactory::createDefault();
    EXPECT_EQ(hasher->digestSize(), 20); // SHA-1
    EXPECT_STREQ(hasher->name(), "sha1");
}

// Test: Hasher factory creates by name
TEST(HasherTest, FactoryCreatesByName) {
    auto sha1 = HasherFactory::create("sha1");
    ASSERT_NE(sha1, nullptr);
    EXPECT_EQ(sha1->digestSize(), 20);
    EXPECT_STREQ(sha1->name(), "sha1");
    
    auto sha256 = HasherFactory::create("sha256");
    ASSERT_NE(sha256, nullptr);
    EXPECT_EQ(sha256->digestSize(), 32);
    EXPECT_STREQ(sha256->name(), "sha256");
}

// Test: Same input produces same hash
TEST(HasherTest, SameInputSameHash) {
    Sha1Hasher hasher1, hasher2;
    
    hasher1.update("test");
    hasher2.update("test");
    
    auto digest1 = hasher1.digest();
    auto digest2 = hasher2.digest();
    
    EXPECT_EQ(digest1, digest2);
}

// Test: Different input produces different hash
TEST(HasherTest, DifferentInputDifferentHash) {
    Sha1Hasher hasher;
    
    hasher.update("test1");
    auto digest1 = hasher.digest();
    
    hasher.reset();
    hasher.update("test2");
    auto digest2 = hasher.digest();
    
    EXPECT_NE(digest1, digest2);
}

// Test: Streaming updates
TEST(HasherTest, StreamingUpdates) {
    Sha1Hasher hasher1, hasher2;
    
    // Update in one go
    hasher1.update("hello world");
    auto digest1 = hasher1.digest();
    
    // Update in chunks
    hasher2.update("hello ");
    hasher2.update("world");
    auto digest2 = hasher2.digest();
    
    EXPECT_EQ(digest1, digest2); // Should be same
}

// Test: Reset functionality
TEST(HasherTest, ResetFunctionality) {
    Sha1Hasher hasher;
    
    hasher.update("test");
    auto digest1 = hasher.digest();
    
    // Reset and use again
    hasher.reset();
    hasher.update("test");
    auto digest2 = hasher.digest();
    
    EXPECT_EQ(digest1, digest2); // Should be same after reset
}

// Test: ToHex conversion
TEST(HasherTest, ToHexConversion) {
    std::vector<uint8_t> bytes = {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56};
    std::string hex = IHasher::toHex(bytes);
    
    EXPECT_EQ(hex, "abcdef123456");
}

// Test: Large input
TEST(HasherTest, LargeInput) {
    Sha1Hasher hasher;
    
    std::string large(10000, 'A');
    hasher.update(large);
    auto digest = hasher.digest();
    
    EXPECT_EQ(digest.size(), 20); // Should still be 20 bytes
}

// Test: SHA-1 vs SHA-256 different outputs
TEST(HasherTest, Sha1VsSha256Different) {
    Sha1Hasher sha1;
    Sha256Hasher sha256;
    
    sha1.update("test");
    sha256.update("test");
    
    auto digest1 = sha1.digest();
    auto digest2 = sha256.digest();
    
    EXPECT_NE(digest1, digest2); // Different algorithms = different hashes
    EXPECT_EQ(digest1.size(), 20);
    EXPECT_EQ(digest2.size(), 32);
}

