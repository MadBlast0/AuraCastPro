#pragma once
// =============================================================================
// LicenseValidator.h — RSA-2048 signature verification for license keys
// =============================================================================
#include <string>
#include <cstdint>

namespace aura {

enum class ValidationResult {
    Valid,
    InvalidSignature,
    Expired,
    WrongMachine,
    Malformed
};

struct LicensePayload {
    std::string email;
    std::string tier;        // "Free", "Pro", "Business"
    std::string expiryDate;  // ISO 8601 or "perpetual"
    std::string machineId;   // Empty = any machine allowed
    uint32_t    issueTimestamp{0};
};

class LicenseValidator {
public:
    void init();
    void shutdown();

    // Validate key and populate payload if valid
    ValidationResult validate(const std::string& licenseKey,
                              LicensePayload& outPayload) const;

    // Get current machine fingerprint (CPU serial + disk serial, hashed)
    static std::string getMachineId();

private:
    // Embedded RSA-2048 public key (PEM format, loaded at compile time)
    static const char* kPublicKeyPEM;

    bool verifyRSASignature(const std::string& payload,
                            const std::string& signature) const;
    static std::string decodeLicenseKey(const std::string& key);
};

} // namespace aura
