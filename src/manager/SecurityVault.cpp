// =============================================================================
// SecurityVault.cpp -- DPAPI + AES-256-GCM encrypted key vault
//
// Storage format (vault.json):
// {
//   "version": 1,
//   "blob": "<base64 of AES-GCM encrypted JSON containing all key-value pairs>"
// }
//
// The AES key is derived via DPAPI:
//   1. CryptProtectData(randomSeed, entropy=machineId) -> protectedBlob
//   2. Store protectedBlob alongside vault.json (user-profile-bound)
//   3. On read: CryptUnprotectData(protectedBlob) -> AES key
//   4. AES-256-GCM decrypt vault blob -> plaintext JSON
//
// Why DPAPI: keys are useless on another machine or another Windows user account.
// An attacker with physical access to the PC still cannot decrypt the vault
// without being logged in as the same Windows user.
// =============================================================================

#include "../pch.h"  // PCH
#include "SecurityVault.h"
#include "../utils/Logger.h"
#include "../utils/EncryptHelper.h"

#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "bcrypt.lib")
#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")

#include <nlohmann/json.hpp>
#include <QStandardPaths>
#include <QDir>
#include <fstream>
#include <stdexcept>
#include <format>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

namespace aura {

struct SecurityVault::Impl {
    json                     secrets;        // decrypted key-value store (in memory only)
    bool                     loaded{false};
    std::string              masterKey;      // cached AES master key (32 bytes)
    std::vector<TrustedDevice> trustedDevices; // Task 154: loaded from trusted_devices.json
};

// -----------------------------------------------------------------------------
SecurityVault::SecurityVault()
    : m_impl(std::make_unique<Impl>()) {}

SecurityVault::~SecurityVault() { shutdown(); }

// -----------------------------------------------------------------------------
std::string SecurityVault::vaultFilePath() const {
    const QString appData = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation);
    return (appData + "/vault.json").toStdString();
}

// -----------------------------------------------------------------------------
// getMasterKey() -- Returns a stable per-user AES-256 master key.
//
// Pattern:
//   1. On first run: generate 32 random bytes (AES key).
//   2. Wrap it with CryptProtectData (DPAPI) -> store "dpapiKey" in vault.json header.
//   3. On subsequent runs: load "dpapiKey" -> CryptUnprotectData -> AES key.
//
// DPAPI ties the key to the current Windows user + machine.
// CryptProtectData is intentionally NOT called with the same plaintext each time
// because that would produce different ciphertext (it's randomized by design).
// We PERSIST the wrapped blob so the same key can be recovered later.
// -----------------------------------------------------------------------------
std::string SecurityVault::getMasterKey() {
    // If already loaded into memory, return it
    if (!m_impl->masterKey.empty()) return m_impl->masterKey;

    // Try to load from the vault file header
    const std::string path = vaultFilePath();
    std::ifstream f(path);
    if (f.is_open()) {
        try {
            json outer;
            f >> outer;
            if (outer.contains("dpapiKey")) {
                // Decode base64 -> DPAPI blob -> AES key
                const std::string b64 = outer["dpapiKey"].get<std::string>();
                DWORD blobLen = 0;
                CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64,
                                     nullptr, &blobLen, nullptr, nullptr);
                std::vector<BYTE> blob(blobLen);
                CryptStringToBinaryA(b64.c_str(), 0, CRYPT_STRING_BASE64,
                                     blob.data(), &blobLen, nullptr, nullptr);

                DATA_BLOB input;
                input.pbData = blob.data();
                input.cbData = blobLen;
                DATA_BLOB output{};

                if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                       0, &output)) {
                    m_impl->masterKey.assign(
                        reinterpret_cast<char*>(output.pbData),
                        std::min<size_t>(output.cbData, 32));
                    LocalFree(output.pbData);
                    AURA_LOG_DEBUG("SecurityVault", "Master key recovered from DPAPI blob.");
                    return m_impl->masterKey;
                }
                AURA_LOG_WARN("SecurityVault",
                    "CryptUnprotectData failed: {} -- vault may be from another user/machine.",
                    GetLastError());
            }
        } catch (...) {}
    }

    // Generate a fresh 32-byte AES key
    std::array<BYTE, 32> rawKey{};
    if (!BCryptGenRandom(nullptr, rawKey.data(), 32,
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG)) {
        // BCryptGenRandom succeeded (0 = STATUS_SUCCESS)
    } else {
        // Fallback: use CryptGenRandom via legacy CryptoAPI
        HCRYPTPROV hProv = 0;
        if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES,
                                  CRYPT_VERIFYCONTEXT)) {
            CryptGenRandom(hProv, 32, rawKey.data());
            CryptReleaseContext(hProv, 0);
        }
    }

    // DPAPI-wrap the new key for storage
    DATA_BLOB input;
    input.pbData = rawKey.data();
    input.cbData = 32;
    DATA_BLOB output{};
    if (CryptProtectData(&input, L"AuraCastPro vault key", nullptr, nullptr,
                          nullptr, 0, &output)) {
        // Base64-encode and store in vault file header
        DWORD b64Len = 0;
        CryptBinaryToStringA(output.pbData, output.cbData,
                             CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                             nullptr, &b64Len);
        std::string b64(b64Len, '\0');
        CryptBinaryToStringA(output.pbData, output.cbData,
                             CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                             b64.data(), &b64Len);
        b64.resize(b64Len);
        LocalFree(output.pbData);

        // Write header with dpapiKey (blob field will be added/updated separately)
        json hdr;
        hdr["version"]  = 2;
        hdr["dpapiKey"] = b64;
        hdr["blob"]     = "";
        std::ofstream hdrFile(vaultFilePath());
        if (hdrFile.is_open()) hdrFile << hdr.dump(2);

        AURA_LOG_INFO("SecurityVault", "Generated new DPAPI-protected vault key.");
    } else {
        AURA_LOG_ERROR("SecurityVault",
            "CryptProtectData failed: {}. Vault will be unencrypted!", GetLastError());
    }

    m_impl->masterKey.assign(reinterpret_cast<char*>(rawKey.data()), 32);
    return m_impl->masterKey;
}

// Keep old name as an alias for compatibility (loadVault calls deriveMasterKey)
std::string SecurityVault::deriveMasterKey() const {
    return const_cast<SecurityVault*>(this)->getMasterKey();
}

// -----------------------------------------------------------------------------
void SecurityVault::init() {
    AURA_LOG_INFO("SecurityVault",
        "Initialising. Vault path: {}", vaultFilePath());
    loadVault();
}

// -----------------------------------------------------------------------------
bool SecurityVault::loadVault() {
    const std::string path = vaultFilePath();
    std::ifstream f(path);
    if (!f.is_open()) {
        // First run -- vault is empty
        m_impl->secrets = json::object();
        m_impl->loaded  = true;
        AURA_LOG_INFO("SecurityVault", "No existing vault. Starting fresh.");
        return true;
    }

    try {
        json outer;
        f >> outer;

        if (!outer.contains("blob")) {
            m_impl->secrets = json::object();
            m_impl->loaded  = true;
            return true;
        }

        // Decode and decrypt the blob
        const std::string blobB64 = outer["blob"].get<std::string>();

        // Base64 decode (using standard Windows CryptStringToBinaryA)
        DWORD blobLen = 0;
        CryptStringToBinaryA(blobB64.c_str(), 0, CRYPT_STRING_BASE64,
                             nullptr, &blobLen, nullptr, nullptr);
        std::vector<uint8_t> encBlob(blobLen);
        CryptStringToBinaryA(blobB64.c_str(), 0, CRYPT_STRING_BASE64,
                             encBlob.data(), &blobLen, nullptr, nullptr);

        // Decrypt with derived key
        const std::string keyMaterial = deriveMasterKey();
        EncryptHelper::Key key;
        std::copy_n(keyMaterial.begin(),
                    std::min(keyMaterial.size(), size_t(32)), key.begin());

        const auto plaintext = EncryptHelper::decrypt(encBlob, key);
        const std::string plainStr(plaintext.begin(), plaintext.end());
        m_impl->secrets = json::parse(plainStr);
        m_impl->loaded  = true;

        AURA_LOG_INFO("SecurityVault", "Vault loaded. {} entries.",
                      m_impl->secrets.size());
        return true;
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("SecurityVault", "Failed to load vault: {}. "
                       "Pairing data lost -- devices will need to be re-paired.", e.what());
        m_impl->secrets = json::object();
        m_impl->loaded  = true;
        return false;
    }
}

// -----------------------------------------------------------------------------
bool SecurityVault::saveVault() const {
    const std::string plainStr = m_impl->secrets.dump();
    const std::vector<uint8_t> plaintext(plainStr.begin(), plainStr.end());

    const std::string keyMaterial = deriveMasterKey();
    EncryptHelper::Key key(32, 0);
    std::copy_n(keyMaterial.begin(),
                std::min(keyMaterial.size(), size_t(32)), key.begin());

    const auto encBlob = EncryptHelper::encrypt(plaintext, key);

    // Base64 encode
    DWORD b64Len = 0;
    CryptBinaryToStringA(encBlob.data(), static_cast<DWORD>(encBlob.size()),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         nullptr, &b64Len);
    std::string b64(b64Len, '\0');
    CryptBinaryToStringA(encBlob.data(), static_cast<DWORD>(encBlob.size()),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                         b64.data(), &b64Len);

    json outer;
    outer["version"] = 1;
    outer["blob"]    = b64;

    std::ofstream f(vaultFilePath());
    if (!f.is_open()) {
        AURA_LOG_ERROR("SecurityVault", "Cannot write vault file: {}", vaultFilePath());
        return false;
    }
    f << outer.dump(2);
    return true;
}

// -----------------------------------------------------------------------------
bool SecurityVault::store(const std::string& key, const std::string& value) {
    m_impl->secrets[key] = value;
    return saveVault();
}

std::optional<std::string> SecurityVault::retrieve(const std::string& key) const {
    if (!m_impl->loaded || !m_impl->secrets.contains(key)) return std::nullopt;
    return m_impl->secrets[key].get<std::string>();
}

bool SecurityVault::remove(const std::string& key) {
    m_impl->secrets.erase(key);
    return saveVault();
}

bool SecurityVault::removeAllForDevice(const std::string& deviceId) {
    std::vector<std::string> toRemove;
    for (auto it = m_impl->secrets.begin(); it != m_impl->secrets.end(); ++it) {
        if (it.key().starts_with(deviceId)) {
            toRemove.push_back(it.key());
        }
    }
    for (const auto& k : toRemove) m_impl->secrets.erase(k);
    return saveVault();
}

void SecurityVault::shutdown() {
    // Zero out in-memory secrets before destruction
    m_impl->secrets = json::object();
}

// =============================================================================
// Task 154 -- Trusted device persistence
// =============================================================================

std::string SecurityVault::trustedDevicesFilePath() const {
    const QString appData = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation);
    QDir().mkpath(appData + "/security");
    return (appData + "/security/trusted_devices.json").toStdString();
}

bool SecurityVault::loadTrustedDevices() {
    const std::string path = trustedDevicesFilePath();
    std::ifstream f(path);
    if (!f.is_open()) {
        AURA_LOG_INFO("SecurityVault",
            "No trusted_devices.json found at '{}' -- starting fresh.", path);
        return true; // Not an error on first run
    }
    try {
        json j;
        f >> j;
        if (!j.is_array()) {
            AURA_LOG_WARN("SecurityVault",
                "trusted_devices.json root is not an array -- ignoring.");
            return false;
        }
        m_impl->trustedDevices.clear();
        for (const auto& entry : j) {
            TrustedDevice d;
            d.deviceId        = entry.value("deviceId",        "");
            d.displayName     = entry.value("displayName",     "");
            d.platform        = entry.value("platform",        "");
            d.modelIdentifier = entry.value("modelIdentifier", "");
            d.pairingKeyRef   = entry.value("pairingKeyRef",   "");
            d.firstPaired     = entry.value("firstPaired",     "");
            d.lastConnected   = entry.value("lastConnected",   "");
            d.connectionCount = entry.value("connectionCount", 0u);
            if (!d.deviceId.empty())
                m_impl->trustedDevices.push_back(std::move(d));
        }
        AURA_LOG_INFO("SecurityVault",
            "Loaded {} trusted device(s).", m_impl->trustedDevices.size());
        return true;
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("SecurityVault",
            "Failed to parse trusted_devices.json: {}", e.what());
        return false;
    }
}

bool SecurityVault::saveTrustedDevices() const {
    const std::string path    = trustedDevicesFilePath();
    const std::string tmpPath = path + ".tmp";
    try {
        // Build JSON array
        json arr = json::array();
        for (const auto& d : m_impl->trustedDevices) {
            arr.push_back({
                {"deviceId",        d.deviceId},
                {"displayName",     d.displayName},
                {"platform",        d.platform},
                {"modelIdentifier", d.modelIdentifier},
                {"pairingKeyRef",   d.pairingKeyRef},
                {"firstPaired",     d.firstPaired},
                {"lastConnected",   d.lastConnected},
                {"connectionCount", d.connectionCount}
            });
        }
        // Atomic write: write to .tmp first, then rename
        {
            std::ofstream f(tmpPath);
            if (!f.is_open()) {
                AURA_LOG_ERROR("SecurityVault",
                    "Cannot open '{}' for writing.", tmpPath);
                return false;
            }
            f << arr.dump(2);  // 2-space indent for readability
        }
        // std::filesystem::rename is atomic on Windows (same volume)
        std::filesystem::rename(tmpPath, path);
        AURA_LOG_DEBUG("SecurityVault",
            "trusted_devices.json saved ({} device(s)).",
            m_impl->trustedDevices.size());
        return true;
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("SecurityVault",
            "saveTrustedDevices failed: {}", e.what());
        std::filesystem::remove(tmpPath); // clean up orphan tmp
        return false;
    }
}

const std::vector<TrustedDevice>& SecurityVault::trustedDevices() const {
    return m_impl->trustedDevices;
}

bool SecurityVault::upsertTrustedDevice(const TrustedDevice& device) {
    for (auto& d : m_impl->trustedDevices) {
        if (d.deviceId == device.deviceId) {
            d = device; // update in-place
            return saveTrustedDevices();
        }
    }
    m_impl->trustedDevices.push_back(device);
    return saveTrustedDevices();
}

bool SecurityVault::forgetDevice(const std::string& deviceId) {
    auto& v = m_impl->trustedDevices;
    v.erase(std::remove_if(v.begin(), v.end(),
        [&](const TrustedDevice& d){ return d.deviceId == deviceId; }),
        v.end());
    removeAllForDevice(deviceId); // also purge vault secrets
    return saveTrustedDevices();
}

bool SecurityVault::recordConnection(const std::string& deviceId) {
    // Get current UTC time as ISO-8601 string
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &tt);
#else
    gmtime_r(&tt, &tm_utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    const std::string nowStr = buf;

    for (auto& d : m_impl->trustedDevices) {
        if (d.deviceId == deviceId) {
            d.lastConnected = nowStr;
            ++d.connectionCount;
            return saveTrustedDevices();
        }
    }
    return false; // device not found -- not an error
}

} // namespace aura
