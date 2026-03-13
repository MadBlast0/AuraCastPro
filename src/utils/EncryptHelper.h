#pragma once
// =============================================================================
// EncryptHelper.h -- AES-256-GCM encryption / decryption helpers.
//
// Used by SecurityVault to encrypt pairing keys and license data at rest.
// All operations use OpenSSL EVP API (constant-time, side-channel resistant).
//
// Format of encrypted blobs:
//   [12 bytes IV][16 bytes GCM tag][N bytes ciphertext]
// =============================================================================

#include <vector>
#include <cstdint>
#include <string>
#include <span>

namespace aura {

class EncryptHelper {
public:
    // Key must be exactly 32 bytes (256-bit AES key)
    using Key = std::vector<uint8_t>;

    // -----------------------------------------------------------------------
    // Generate a cryptographically random key (32 bytes)
    // -----------------------------------------------------------------------
    static Key generateKey();

    // -----------------------------------------------------------------------
    // Derive a 256-bit key from a passphrase using PBKDF2-SHA256
    //   passphrase : UTF-8 string
    //   salt       : random salt (at least 16 bytes)
    //   iterations : OWASP recommendation for PBKDF2-SHA256 is ≥ 600,000
    // -----------------------------------------------------------------------
    static Key deriveKey(const std::string& passphrase,
                         const std::vector<uint8_t>& salt,
                         int iterations = 600000);

    // -----------------------------------------------------------------------
    // Encrypt plaintext using AES-256-GCM.
    //   Returns: [12 IV][16 tag][ciphertext]
    //   Throws on any OpenSSL error.
    // -----------------------------------------------------------------------
    static std::vector<uint8_t> encrypt(std::span<const uint8_t> plaintext,
                                        const Key& key);

    // -----------------------------------------------------------------------
    // Decrypt a blob produced by encrypt().
    //   Throws if authentication tag verification fails (tampering detected).
    // -----------------------------------------------------------------------
    static std::vector<uint8_t> decrypt(std::span<const uint8_t> blob,
                                        const Key& key);

    // -----------------------------------------------------------------------
    // Constant-time memory comparison (prevents timing attacks)
    // -----------------------------------------------------------------------
    static bool secureCompare(std::span<const uint8_t> a,
                               std::span<const uint8_t> b);

    // -----------------------------------------------------------------------
    // Securely zero memory before releasing it
    // -----------------------------------------------------------------------
    static void secureZero(void* ptr, std::size_t size);

private:
    EncryptHelper() = delete;

    static constexpr int IV_SIZE  = 12; // 96-bit IV -- optimal for GCM
    static constexpr int TAG_SIZE = 16; // 128-bit authentication tag
};

} // namespace aura
