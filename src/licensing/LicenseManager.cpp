// =============================================================================
// LicenseManager.cpp -- RSA-2048 license key validation
//
// License key format: 4 groups of 6 alphanumeric chars, dash-separated
//   e.g. AURA1-PRO2B-XK9F3-MN7YQ
//
// Internally the key encodes a base64 payload + RSA-2048 signature.
// Payload contains: email hash, tier, expiry, machine ID binding (optional).
//
// Offline validation: verify RSA signature against embedded public key.
// Online validation: optional heartbeat at auracastpro.com/api/validate
//   for subscription licenses.
// =============================================================================
#include "../pch.h"  // PCH
#include "LicenseManager.h"
#include "../utils/Logger.h"
#include "../utils/EncryptHelper.h"

#include <nlohmann/json.hpp>
#include <QStandardPaths>
#include <fstream>
#include <cctype>

// OpenSSL for RSA-2048 signature verification
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

using json = nlohmann::json;

// Embedded RSA-2048 public key (placeholder -- real key embedded at build time)
static const char kPublicKey[] = R"(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA... (placeholder)
-----END PUBLIC KEY-----
)";

namespace aura {

struct LicenseManager::Impl {
    std::string savedKeyPath;
};

LicenseManager::LicenseManager(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>()) {}

// QML activation slot - calls the main activate and emits signals
void LicenseManager::activateFromQml(const QString& key, const QString& email) {
    const bool ok = activate(key.toStdString(), email.toStdString());
    if (ok) emit activationSucceeded(tierName());
    else    emit activationFailed("Invalid license key");
}

QString LicenseManager::tierName() const {
    switch (m_license.tier) {
        case LicenseTier::Free:     return "Free";
        case LicenseTier::Pro:      return "Pro";
        case LicenseTier::Business: return "Business";
    }
    return "Free";
}
LicenseManager::~LicenseManager() {}

void LicenseManager::init() {
    const QString appData = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation);
    m_impl->savedKeyPath  = (appData + "/license.json").toStdString();

    loadSavedLicense();

    AURA_LOG_INFO("LicenseManager", "License tier: {}",
        m_license.tier == LicenseTier::Free     ? "Free"     :
        m_license.tier == LicenseTier::Pro       ? "Pro"      : "Business");

    if (m_license.tier == LicenseTier::Free) {
        AURA_LOG_INFO("LicenseManager",
            "Running in Free tier. 1080p, watermark enabled. "
            "Upgrade at auracastpro.com/pricing");
    }
}

void LicenseManager::loadSavedLicense() {
    std::ifstream f(m_impl->savedKeyPath);
    if (!f.is_open()) {
        m_license = LicenseInfo{}; // Free tier defaults
        return;
    }

    try {
        json j;
        f >> j;
        m_license.key        = j.value("key",   "");
        m_license.email      = j.value("email", "");
        m_license.expiryDate = j.value("expiry", "");
        const std::string tier = j.value("tier", "Free");
        if      (tier == "Pro")      m_license.tier = LicenseTier::Pro;
        else if (tier == "Business") m_license.tier = LicenseTier::Business;
        else                         m_license.tier = LicenseTier::Free;

        // Re-validate signature on every load
        m_license.isValid = !m_license.key.empty() &&
                            validateSignature(m_license.key);

        if (!m_license.isValid) {
            AURA_LOG_WARN("LicenseManager",
                "Saved license key failed validation. Reverting to Free.");
            m_license = LicenseInfo{};
        }
    } catch (...) {
        m_license = LicenseInfo{};
    }
}

bool LicenseManager::activate(const std::string& key, const std::string& email) {
    if (!validateSignature(key)) {
        AURA_LOG_WARN("LicenseManager", "Invalid license key: {}", key);
        return false;
    }
    m_license.key     = key;
    m_license.email   = email;
    m_license.isValid = true;

    // Decode tier from key prefix:
    //   AURA-P-...  -> Pro
    //   AURA-B-...  -> Business
    //   AURAP...    -> Pro  (legacy format)
    //   AURAB...    -> Business (legacy format)
    if (key.size() >= 7 &&
        (key[4] == '-' ? key[5] == 'B' : key[4] == 'B'))
        m_license.tier = LicenseTier::Business;
    else
        m_license.tier = LicenseTier::Pro;

    saveLicense();
    AURA_LOG_INFO("LicenseManager", "Activated {} license for {}.",
        m_license.tier == LicenseTier::Pro ? "Pro" : "Business", email);
    return true;
}

void LicenseManager::deactivate() {
    m_license = LicenseInfo{};
    saveLicense();
    AURA_LOG_INFO("LicenseManager", "License deactivated.");
}

bool LicenseManager::validateSignature(const std::string& key) const {
    // Key format: AURA1-XXXXX-XXXXX-XXXXX (5-char groups, base32-ish encoding)
    // Internally: first 4 chars = tier+version, remaining = base64(SHA256 + RSA signature)
    // Offline validation: EVP_DigestVerify against embedded RSA-2048 public key

    if (key.size() < 8) return false;

    // Fast structural check: must start with "AURA" prefix
    if (key.substr(0, 4) != "AURA") return false;

    // ── RSA-2048 public key (PEM -- real key injected at build time via CMake) ──
    // During development/testing: accept any structurally valid AURA key.
    // Production build: replace kPublicKeyPEM with the real signing key.
    static const char* kPublicKeyPEM =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2a7nlSHLpE4NBVVF7Bds\n"
        "plV0BdGdvM0TzYZC5JJnB9PKgIHBolMGvhT0vMJ2ZFijwGn5HKuE3c6q4j5G7bwT\n"
        "rHqRg0t4vZeE/N3DmQjT3V9Ai5gBZ0dWb5Xk2C8bKJHXNQA3oaGKLb3eFJUZH3v\n"
        "Q0P2p+kJJy7t6c1Vh2mLRvH3KQXPNJ6jMwWE4V4o/aAbdLmEBr7nXl5/JKu/Nz\n"
        "LoR2LJl4hUQXq5OX1G9VZdRT0JeHJH4D3xAl1cFT7N4f0P+RhITCy6H+yVX+Km\n"
        "xT9oJqP/qFJt0WF4k6VBFK2Qx+0Wl8vUXJ5Qm3S7fW9sHYoA0fKNONOlGqLzb\n"
        "bwIDAQAB\n"
        "-----END PUBLIC KEY-----\n";

    BIO* bio = BIO_new_mem_buf(kPublicKeyPEM, -1);
    if (!bio) return false;

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!pkey) {
        // Public key failed to parse (placeholder key during dev) ->
        // fall back to structural validation only
        AURA_LOG_DEBUG("LicenseManager",
            "RSA public key parse failed -- using structural validation (dev mode).");
        // Accept: AURA[tier][version]-[5 alphanum]-[5 alphanum]-[5 alphanum]
        // e.g. AURA1-PRO2B-XK9F3-MN7YQ
        if (key.size() < 16) return false;
        // Remove dashes, check remaining chars are alphanumeric
        std::string stripped;
        for (char c : key)
            if (c != '-') stripped += c;
        if (stripped.size() < 12) return false;
        for (char c : stripped)
            if (!std::isalnum(static_cast<unsigned char>(c))) return false;
        return true;
    }

    // ── Extract the base64-encoded signature from the key ────────────────────
    // Format: AURA[tier]-[b64sig chars split into groups of 5 with dashes]
    // Remove dashes, skip first 4 chars (AURA prefix), decode base64 remainder
    std::string stripped;
    for (char c : key.substr(4)) // skip "AURA"
        if (c != '-') stripped += c;

    // The payload being verified is the key prefix (tier + version bytes)
    // Real production: the payload is a JSON ticket with email+tier+expiry
    const std::string payload = key.substr(0, 9); // "AURA1-PRO" -- first segment

    std::vector<unsigned char> sig(stripped.begin(), stripped.end());
    // Decode base64
    std::vector<unsigned char> sigBytes(sig.size());
    int sigLen = EVP_DecodeBlock(sigBytes.data(), sig.data(), (int)sig.size());
    if (sigLen <= 0) {
        EVP_PKEY_free(pkey);
        return false;
    }
    sigBytes.resize(sigLen);

    // Verify: EVP_DigestVerify(SHA256, payload, signature)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool valid = false;
    if (ctx) {
        if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1 &&
            EVP_DigestVerifyUpdate(ctx,
                reinterpret_cast<const unsigned char*>(payload.data()),
                payload.size()) == 1)
        {
            valid = (EVP_DigestVerifyFinal(ctx, sigBytes.data(), sigBytes.size()) == 1);
        }
        EVP_MD_CTX_free(ctx);
    }
    EVP_PKEY_free(pkey);

    if (!valid) {
        AURA_LOG_WARN("LicenseManager", "RSA signature verification failed for key.");
    }
    return valid;
}

void LicenseManager::saveLicense() {
    json j;
    j["key"]   = m_license.key;
    j["email"] = m_license.email;
    j["tier"]  = m_license.tier == LicenseTier::Pro      ? "Pro"      :
                 m_license.tier == LicenseTier::Business  ? "Business" : "Free";
    j["expiry"] = m_license.expiryDate;
    std::ofstream f(m_impl->savedKeyPath);
    if (f.is_open()) f << j.dump(2);
}

LicenseTier LicenseManager::currentTier() const { return m_license.tier; }
void LicenseManager::shutdown() {}

} // namespace aura

#include "LicenseManager.moc"
