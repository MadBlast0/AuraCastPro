#pragma once
// =============================================================================
// MirrorDeviceInfo.h -- Device information for mirror windows
// Shared between DeviceMirrorWindow and DeviceMirrorManager
// =============================================================================
#include <string>
#include <cstdint>

namespace aura {

struct MirrorDeviceInfo {
    std::string deviceId;
    std::string deviceName;
    std::string ipAddress;
    std::string videoCodec;
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t fps{60};
    float bitrate{20.0f};
};

} // namespace aura
