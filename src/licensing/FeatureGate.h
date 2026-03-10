#pragma once
// =============================================================================
// FeatureGate.h — Runtime feature availability based on license tier
// =============================================================================
#include <functional>
#include <string>

namespace aura {

enum class Feature {
    Resolution4K,
    FrameRate120,
    HardwareDecode,
    VirtualCamera,
    Recording,
    RTMPStreaming,
    MultiDevice,
    InputControl,
    HDR,
    NoWatermark,
    PluginSupport,
    APIAccess,
};

class LicenseManager;

class FeatureGate {
public:
    explicit FeatureGate(const LicenseManager* mgr);

    void init();
    void shutdown();

    bool isEnabled(Feature feature) const;
    std::string featureName(Feature feature) const;

    // Returns a user-facing upgrade message for a locked feature
    std::string upgradeMessage(Feature feature) const;

private:
    const LicenseManager* m_licenseManager;
};

} // namespace aura
