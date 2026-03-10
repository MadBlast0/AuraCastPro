// =============================================================================
// SecurityTests.cpp — Unit tests for EncryptHelper (AES-256-GCM)
// =============================================================================

#include <gtest/gtest.h>
#include "../../src/utils/EncryptHelper.h"

#include <vector>
#include <string>

using namespace aura;

// ─── Key generation ───────────────────────────────────────────────────────────

TEST(EncryptHelperTest, GeneratesCorrectKeyLength) {
    auto key = EncryptHelper::generateKey();
    EXPECT_EQ(key.size(), 32u); // AES-256 = 32 bytes
}

TEST(EncryptHelperTest, GeneratedKeysAreUnique) {
    auto k1 = EncryptHelper::generateKey();
    auto k2 = EncryptHelper::generateKey();
    EXPECT_NE(k1, k2); // Statistically certain with 256-bit random keys
}

// ─── Key derivation ───────────────────────────────────────────────────────────

TEST(EncryptHelperTest, DeriveKeyIsDeterministic) {
    const std::string pass = "test-passphrase";
    const std::vector<uint8_t> salt(16, 0xAB);

    auto k1 = EncryptHelper::deriveKey(pass, salt, 1000); // Low iterations for tests
    auto k2 = EncryptHelper::deriveKey(pass, salt, 1000);

    EXPECT_EQ(k1, k2);
}

TEST(EncryptHelperTest, DifferentPasswordGivesDifferentKey) {
    const std::vector<uint8_t> salt(16, 0x01);
    auto k1 = EncryptHelper::deriveKey("password1", salt, 1000);
    auto k2 = EncryptHelper::deriveKey("password2", salt, 1000);
    EXPECT_NE(k1, k2);
}

TEST(EncryptHelperTest, DifferentSaltGivesDifferentKey) {
    const std::vector<uint8_t> s1(16, 0x01);
    const std::vector<uint8_t> s2(16, 0x02);
    auto k1 = EncryptHelper::deriveKey("password", s1, 1000);
    auto k2 = EncryptHelper::deriveKey("password", s2, 1000);
    EXPECT_NE(k1, k2);
}

// ─── Encrypt / Decrypt round-trip ─────────────────────────────────────────────

TEST(EncryptHelperTest, EncryptDecryptRoundTrip) {
    auto key = EncryptHelper::generateKey();
    const std::string plaintext = "AuraCastPro secure pairing data";
    const std::vector<uint8_t> input(plaintext.begin(), plaintext.end());

    auto encrypted = EncryptHelper::encrypt(input, key);
    auto decrypted = EncryptHelper::decrypt(encrypted, key);

    EXPECT_EQ(decrypted, input);
}

TEST(EncryptHelperTest, EmptyPlaintextRoundTrip) {
    auto key = EncryptHelper::generateKey();
    std::vector<uint8_t> empty;

    auto encrypted = EncryptHelper::encrypt(empty, key);
    auto decrypted = EncryptHelper::decrypt(encrypted, key);

    EXPECT_EQ(decrypted, empty);
}

TEST(EncryptHelperTest, LargePlaintextRoundTrip) {
    auto key = EncryptHelper::generateKey();
    std::vector<uint8_t> large(1024 * 1024, 0x42); // 1 MB

    auto encrypted = EncryptHelper::encrypt(large, key);
    auto decrypted = EncryptHelper::decrypt(encrypted, key);

    EXPECT_EQ(decrypted, large);
}

// ─── Encryption produces unique ciphertext (random IV) ───────────────────────

TEST(EncryptHelperTest, TwoEncryptionsProduceDifferentCiphertext) {
    auto key = EncryptHelper::generateKey();
    const std::vector<uint8_t> input = {0x01, 0x02, 0x03, 0x04};

    auto c1 = EncryptHelper::encrypt(input, key);
    auto c2 = EncryptHelper::encrypt(input, key);

    // Different IV → different ciphertext (even for same plaintext + key)
    EXPECT_NE(c1, c2);
}

// ─── Tamper detection via GCM tag ─────────────────────────────────────────────

TEST(EncryptHelperTest, TamperedCiphertextThrows) {
    auto key = EncryptHelper::generateKey();
    const std::vector<uint8_t> input = {0xDE, 0xAD, 0xBE, 0xEF};

    auto encrypted = EncryptHelper::encrypt(input, key);

    // Flip a bit in the ciphertext (last byte)
    encrypted.back() ^= 0xFF;

    EXPECT_THROW(EncryptHelper::decrypt(encrypted, key), std::runtime_error);
}

TEST(EncryptHelperTest, WrongKeyThrows) {
    auto key1 = EncryptHelper::generateKey();
    auto key2 = EncryptHelper::generateKey();

    const std::vector<uint8_t> input = {0x01, 0x02, 0x03};
    auto encrypted = EncryptHelper::encrypt(input, key1);

    EXPECT_THROW(EncryptHelper::decrypt(encrypted, key2), std::runtime_error);
}

// ─── Minimum blob size validation ────────────────────────────────────────────

TEST(EncryptHelperTest, TooSmallBlobThrows) {
    auto key = EncryptHelper::generateKey();
    const std::vector<uint8_t> tiny = {0x01, 0x02}; // Less than IV(12) + tag(16)

    EXPECT_THROW(EncryptHelper::decrypt(tiny, key), std::invalid_argument);
}

// ─── Secure compare ──────────────────────────────────────────────────────────

TEST(EncryptHelperTest, SecureCompareEqual) {
    const std::vector<uint8_t> a = {1, 2, 3, 4};
    const std::vector<uint8_t> b = {1, 2, 3, 4};
    EXPECT_TRUE(EncryptHelper::secureCompare(a, b));
}

TEST(EncryptHelperTest, SecureCompareNotEqual) {
    const std::vector<uint8_t> a = {1, 2, 3, 4};
    const std::vector<uint8_t> b = {1, 2, 3, 5};
    EXPECT_FALSE(EncryptHelper::secureCompare(a, b));
}

TEST(EncryptHelperTest, SecureCompareDifferentSizes) {
    const std::vector<uint8_t> a = {1, 2, 3};
    const std::vector<uint8_t> b = {1, 2, 3, 4};
    EXPECT_FALSE(EncryptHelper::secureCompare(a, b));
}
