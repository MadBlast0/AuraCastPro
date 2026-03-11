// FeatureGate.cpp
#include "../pch.h"  // PCH
#include "FeatureGate.h"
#include "LicenseManager.h"
#include "../utils/Logger.h"
#include <format>

namespace aura {

FeatureGate::FeatureGate(const LicenseManager* mgr) : m_licenseManager(mgr) {}

void FeatureGate::init() {
    AURA_LOG_INFO("FeatureGate", "Initialised. Feature gates bound to LicenseManager.");
}
void FeatureGate::shutdown() {}

bool FeatureGate::isEnabled(Feature feature) const {
    if (!m_licenseManager) return false;
    switch (feature) {
        case Feature::Resolution4K:    return m_licenseManager->canUse4K();
        case Feature::Recording:       return m_licenseManager->canRecord();
        case Feature::VirtualCamera:   return m_licenseManager->canUseVirtualCamera();
        case Feature::RTMPStreaming:    return m_licenseManager->canStreamRTMP();
        case Feature::MultiDevice:     return m_licenseManager->canConnectMultiple();
        case Feature::NoWatermark:     return !m_licenseManager->showsWatermark();
        case Feature::FrameRate120:    return m_licenseManager->canUse4K(); // Pro+
        case Feature::HardwareDecode:  return true; // always available
        case Feature::InputControl:    return true; // always available
        case Feature::HDR:             return m_licenseManager->canUse4K();
        case Feature::PluginSupport:   return m_licenseManager->canStreamRTMP(); // Business
        case Feature::APIAccess:       return m_licenseManager->canStreamRTMP(); // Business
    }
    return false;
}

std::string FeatureGate::featureName(Feature feature) const {
    switch (feature) {
        case Feature::Resolution4K:  return "4K Resolution";
        case Feature::Recording:     return "Screen Recording";
        case Feature::VirtualCamera: return "Virtual Camera";
        case Feature::RTMPStreaming: return "RTMP Live Streaming";
        case Feature::MultiDevice:   return "Multi-Device";
        case Feature::NoWatermark:   return "No Watermark";
        case Feature::FrameRate120:  return "120 FPS";
        case Feature::HDR:           return "HDR10";
        case Feature::PluginSupport: return "Plugin Support";
        case Feature::APIAccess:     return "API Access";
        default:                     return "Feature";
    }
}

std::string FeatureGate::upgradeMessage(Feature feature) const {
    const std::string name = featureName(feature);
    const bool needsBusiness =
        (feature == Feature::RTMPStreaming ||
         feature == Feature::MultiDevice   ||
         feature == Feature::PluginSupport ||
         feature == Feature::APIAccess);

    return std::format(
        "{} requires AuraCastPro {}. Upgrade at auracastpro.com/pricing",
        name, needsBusiness ? "Business" : "Pro");
}

} // namespace aura
