#pragma once
// =============================================================================
// HardwareProfiler.h -- Detects GPU capabilities, CPU core count, RAM, and
//                       DirectX 12 feature level at startup.
//
// Results are used to:
//   - Enable/disable hardware decode (HEVC, AV1 on supported GPUs)
//   - Select optimal thread pool sizes
//   - Warn if hardware does not meet minimum requirements
// =============================================================================

#include <string>
#include <vector>
#include <cstdint>

namespace aura {

struct GpuInfo {
    std::string name;           // e.g. "NVIDIA GeForce RTX 4080"
    std::string vendorId;       // "10DE" = NVIDIA, "1002" = AMD, "8086" = Intel
    uint64_t    dedicatedVRAM{};// Bytes of dedicated GPU memory
    bool        supportsD3D12{};
    bool        supportsHWDecodeHEVC{};
    bool        supportsHWDecodeAV1{};
    bool        supportsNVENC{};
    int         d3d12FeatureLevel{}; // e.g. 0xC000 = D3D_FEATURE_LEVEL_12_0
};

struct HardwareProfile {
    std::vector<GpuInfo> gpus;
    GpuInfo*             primaryGpu{nullptr}; // Points into gpus[0] after detect()
    uint32_t             cpuCoreCount{};
    uint64_t             totalRamBytes{};
    std::string          cpuName;
    bool                 meetsMinimumRequirements{};
};

class HardwareProfiler {
public:
    // Call once from main() after Logger::init() and before DX12Manager::init()
    static void detect();

    // Access cached profile (valid after detect())
    static const HardwareProfile& profile();

    // Convenience queries
    static bool hasNVENC();
    static bool hasHWDecodeHEVC();
    static bool hasHWDecodeAV1();
    static uint64_t primaryVRAMBytes();

private:
    HardwareProfiler() = delete;

    static HardwareProfile s_profile;
    static bool            s_detected;

    static void detectGPUs();
    static void detectCPU();
    static void detectRAM();
    static void checkHWCodecSupport(GpuInfo& gpu);
};

} // namespace aura
