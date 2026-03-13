// =============================================================================
// EncryptHelper.cpp -- AES-256-GCM via OpenSSL EVP API
// =============================================================================

#include "../pch.h"  // PCH
#include "EncryptHelper.h"
#include "Logger.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <stdexcept>
#include <cstring>

namespace aura {

namespace {

// Helper: throw with OpenSSL error message
void throwOpenSSLError(const char* context) {
    char buf[256]{};
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    throw std::runtime_error(std::string(context) + ": " + buf);
}

} // anonymous namespace

// -----------------------------------------------------------------------------
EncryptHelper::Key EncryptHelper::generateKey() {
    Key key(32);
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
        throwOpenSSLError("EncryptHelper::generateKey RAND_bytes");
    }
    return key;
}

// -----------------------------------------------------------------------------
EncryptHelper::Key EncryptHelper::deriveKey(const std::string& passphrase,
                                             const std::vector<uint8_t>& salt,
                                             int iterations) {
    Key key(32);
    if (!PKCS5_PBKDF2_HMAC(passphrase.data(), static_cast<int>(passphrase.size()),
                             salt.data(), static_cast<int>(salt.size()),
                             iterations, EVP_sha256(),
                             static_cast<int>(key.size()), key.data())) {
        throwOpenSSLError("EncryptHelper::deriveKey PBKDF2");
    }
    return key;
}

// -----------------------------------------------------------------------------
// Encrypted blob layout: [12 IV][16 tag][ciphertext]
// -----------------------------------------------------------------------------
std::vector<uint8_t> EncryptHelper::encrypt(std::span<const uint8_t> plaintext,
                                             const Key& key) {
    if (key.size() != 32) {
        throw std::invalid_argument("EncryptHelper::encrypt: key must be 32 bytes");
    }

    // Generate random IV
    uint8_t iv[IV_SIZE];
    if (RAND_bytes(iv, IV_SIZE) != 1) {
        throwOpenSSLError("EncryptHelper::encrypt RAND_bytes IV");
    }

    // Allocate output: IV + tag + ciphertext
    std::vector<uint8_t> blob(IV_SIZE + TAG_SIZE + plaintext.size());

    // Copy IV into blob header
    std::memcpy(blob.data(), iv, IV_SIZE);

    // Encrypt
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throwOpenSSLError("EncryptHelper::encrypt EVP_CIPHER_CTX_new");

    // RAII cleanup
    struct CtxGuard { EVP_CIPHER_CTX* c; ~CtxGuard() { EVP_CIPHER_CTX_free(c); } } guard{ctx};

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        throwOpenSSLError("EVP_EncryptInit_ex");

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1)
        throwOpenSSLError("EVP_EncryptInit_ex (key+iv)");

    int outLen = 0;
    uint8_t* cipherStart = blob.data() + IV_SIZE + TAG_SIZE;

    if (EVP_EncryptUpdate(ctx, cipherStart, &outLen,
                          plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
        throwOpenSSLError("EVP_EncryptUpdate");
    }

    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx, cipherStart + outLen, &finalLen) != 1)
        throwOpenSSLError("EVP_EncryptFinal_ex");

    // Extract GCM tag into blob
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, blob.data() + IV_SIZE) != 1)
        throwOpenSSLError("EVP_CTRL_GCM_GET_TAG");

    blob.resize(IV_SIZE + TAG_SIZE + outLen + finalLen);
    return blob;
}

// -----------------------------------------------------------------------------
std::vector<uint8_t> EncryptHelper::decrypt(std::span<const uint8_t> blob,
                                             const Key& key) {
    if (key.size() != 32) {
        throw std::invalid_argument("EncryptHelper::decrypt: key must be 32 bytes");
    }
    if (blob.size() < static_cast<std::size_t>(IV_SIZE + TAG_SIZE)) {
        throw std::invalid_argument("EncryptHelper::decrypt: blob too small");
    }

    const uint8_t* iv  = blob.data();
    const uint8_t* tag = blob.data() + IV_SIZE;
    const uint8_t* ciphertext = blob.data() + IV_SIZE + TAG_SIZE;
    const int cipherLen = static_cast<int>(blob.size()) - IV_SIZE - TAG_SIZE;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throwOpenSSLError("EncryptHelper::decrypt EVP_CIPHER_CTX_new");
    struct CtxGuard { EVP_CIPHER_CTX* c; ~CtxGuard() { EVP_CIPHER_CTX_free(c); } } guard{ctx};

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        throwOpenSSLError("EVP_DecryptInit_ex");

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) != 1)
        throwOpenSSLError("EVP_DecryptInit_ex (key+iv)");

    std::vector<uint8_t> plaintext(cipherLen);
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &outLen, ciphertext, cipherLen) != 1)
        throwOpenSSLError("EVP_DecryptUpdate");

    // Set expected tag before calling Final
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                             const_cast<uint8_t*>(tag)) != 1) {
        throwOpenSSLError("EVP_CTRL_GCM_SET_TAG");
    }

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen, &finalLen) != 1) {
        // Tag mismatch = authentication failure = data tampered
        throw std::runtime_error("EncryptHelper::decrypt: GCM authentication tag mismatch -- "
                                 "data may have been tampered with.");
    }

    plaintext.resize(outLen + finalLen);
    return plaintext;
}

// -----------------------------------------------------------------------------
bool EncryptHelper::secureCompare(std::span<const uint8_t> a,
                                   std::span<const uint8_t> b) {
    if (a.size() != b.size()) return false;
    // CRYPTO_memcmp is constant-time -- prevents timing side-channel attacks
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// -----------------------------------------------------------------------------
void EncryptHelper::secureZero(void* ptr, std::size_t size) {
    // OPENSSL_cleanse is guaranteed not to be optimised away by the compiler
    OPENSSL_cleanse(ptr, size);
}

} // namespace aura
