#pragma once
// =============================================================================
// SecurityVault.h — Encrypted secure key storage using Windows DPAPI
//
// Stores device pairing keys and secrets in an AES-256-GCM encrypted JSON file.
// The encryption key itself is protected by Windows DPAPI (CryptProtectData),
// which ties it to the current Windows user account.
//
// Task 154: Also manages trusted_devices.json — atomic write (tmp → rename),
//           loaded on startup and updated on each pair / connect / forget.
// =============================================================================
#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <cstdint>

namespace aura {

// ── Trusted device record (matches security/trusted_devices_schema.json) ────
struct TrustedDevice {
    std::string deviceId;           // e.g. "AA:BB:CC:DD:EE:FF"
    std::string displayName;        // e.g. "John's iPhone 15 Pro"
    std::string platform;           // "iOS" | "Android"
    std::string modelIdentifier;    // e.g. "iPhone15,2"
    std::string pairingKeyRef;      // vault key id storing Ed25519 public key
    std::string firstPaired;        // ISO-8601 UTC
    std::string lastConnected;      // ISO-8601 UTC
    uint32_t    connectionCount{0};
};

class SecurityVault {
public:
    SecurityVault();
    ~SecurityVault();

    void init();
    void shutdown();

    // Store a secret. key = logical name, value = secret bytes (base64 stored)
    bool store(const std::string& key, const std::string& value);

    // Retrieve a secret. Returns nullopt if not found.
    std::optional<std::string> retrieve(const std::string& key) const;

    // Remove a stored secret
    bool remove(const std::string& key);

    // Remove ALL secrets for a device (on forget device)
    bool removeAllForDevice(const std::string& deviceId);

    // ── Task 154: Trusted device persistence ─────────────────────────────────
    // Load trusted_devices.json from AppData/security/ on startup.
    bool loadTrustedDevices();

    // Atomically save trusted_devices.json (write .tmp then rename).
    // Returns false on I/O error (logs details).
    bool saveTrustedDevices() const;

    // In-memory device list
    const std::vector<TrustedDevice>& trustedDevices() const;

    // Add or update a device (matched by deviceId). Triggers saveTrustedDevices().
    bool upsertTrustedDevice(const TrustedDevice& device);

    // Remove a device and its pairing key. Triggers saveTrustedDevices().
    bool forgetDevice(const std::string& deviceId);

    // Update lastConnected timestamp and increment connectionCount.
    bool recordConnection(const std::string& deviceId);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string vaultFilePath()          const;
    std::string trustedDevicesFilePath() const;
    bool loadVault();
    bool saveVault() const;
    std::string deriveMasterKey() const;
    std::string getMasterKey();    // persistent DPAPI-wrapped key   // Uses DPAPI
};

} // namespace aura
