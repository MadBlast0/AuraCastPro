// =============================================================================
// HardwareProfiler.cpp — GPU, CPU, and RAM capability detection
// Uses DXGI to enumerate adapters and Windows API for CPU/RAM info.
// =============================================================================

#include "../pch.h"  // PCH
#include "HardwareProfiler.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <format>
#include <stdexcept>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

using Microsoft::WRL::ComPtr;

namespace aura {

HardwareProfile HardwareProfiler::s_profile{};
bool            HardwareProfiler::s_detected = false;

// -----------------------------------------------------------------------------
void HardwareProfiler::detect() {
    AURA_LOG_INFO("HardwareProfiler", "Detecting hardware capabilities...");
    detectGPUs();
    detectCPU();
    detectRAM();

    // Determine if minimum requirements are met:
    // - At least one D3D12-capable GPU
    // - At least 4 GB RAM
    // - At least 4 CPU cores
    const bool hasD3D12 = !s_profile.gpus.empty() && s_profile.gpus[0].supportsD3D12;
    const bool hasEnoughRAM = s_profile.totalRamBytes >= (4ULL * 1024 * 1024 * 1024);
    const bool hasEnoughCores = s_profile.cpuCoreCount >= 4;

    s_profile.meetsMinimumRequirements = hasD3D12 && hasEnoughRAM && hasEnoughCores;

    if (!s_profile.meetsMinimumRequirements) {
        AURA_LOG_WARN("HardwareProfiler",
            "System does not meet minimum requirements. "
            "D3D12={} RAM≥4GB={} Cores≥4={}",
            hasD3D12, hasEnoughRAM, hasEnoughCores);
    } else {
        AURA_LOG_INFO("HardwareProfiler", "System meets minimum requirements.");
    }

    s_detected = true;
}

// -----------------------------------------------------------------------------
void HardwareProfiler::detectGPUs() {
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        AURA_LOG_ERROR("HardwareProfiler", "CreateDXGIFactory2 failed: {:08X}", (uint32_t)hr);
        return;
    }

    // Enumerate all adapters, preferring high-performance (discrete) GPUs first
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         factory->EnumAdapterByGpuPreference(
             i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++i) {

        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);

        // Skip software/WARP adapters — we want real hardware only
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        GpuInfo gpu;

        // Convert wstring name to UTF-8
        char nameBuf[256]{};
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
        gpu.name = nameBuf;

        gpu.dedicatedVRAM = desc.DedicatedVideoMemory;
        gpu.vendorId      = std::format("{:04X}", desc.VendorId);
        gpu.supportsNVENC = (desc.VendorId == 0x10DE); // NVIDIA

        // Test D3D12 support by trying to create a device
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        gpu.supportsD3D12 = SUCCEEDED(hr);

        if (gpu.supportsD3D12) {
            gpu.d3d12FeatureLevel = D3D_FEATURE_LEVEL_12_0;
        }

        checkHWCodecSupport(gpu);

        AURA_LOG_INFO("HardwareProfiler",
            "GPU[{}]: {} | VRAM: {} MB | D3D12: {} | NVENC: {} | HEVC: {} | AV1: {}",
            i, gpu.name,
            gpu.dedicatedVRAM / (1024 * 1024),
            gpu.supportsD3D12 ? "yes" : "no",
            gpu.supportsNVENC ? "yes" : "no",
            gpu.supportsHWDecodeHEVC ? "yes" : "no",
            gpu.supportsHWDecodeAV1 ? "yes" : "no");

        s_profile.gpus.push_back(std::move(gpu));
    }

    if (!s_profile.gpus.empty()) {
        s_profile.primaryGpu = &s_profile.gpus[0];
    }
}

// -----------------------------------------------------------------------------
void HardwareProfiler::detectCPU() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    s_profile.cpuCoreCount = si.dwNumberOfProcessors;

    // Read CPU brand string from registry (most reliable cross-version method)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR brand[256]{};
        DWORD size = sizeof(brand);
        RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(brand), &size);
        RegCloseKey(hKey);

        char nameBuf[256]{};
        WideCharToMultiByte(CP_UTF8, 0, brand, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
        s_profile.cpuName = nameBuf;
    }

    AURA_LOG_INFO("HardwareProfiler", "CPU: {} ({} logical cores)",
                  s_profile.cpuName, s_profile.cpuCoreCount);
}

// -----------------------------------------------------------------------------
void HardwareProfiler::detectRAM() {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    s_profile.totalRamBytes = ms.ullTotalPhys;

    AURA_LOG_INFO("HardwareProfiler", "RAM: {} MB total ({} MB available)",
                  ms.ullTotalPhys / (1024 * 1024),
                  ms.ullAvailPhys / (1024 * 1024));
}

// -----------------------------------------------------------------------------
void HardwareProfiler::checkHWCodecSupport(GpuInfo& gpu) {
    // Hardware codec support is determined by vendor and approximate generation.
    // A more precise check would use Windows Media Foundation's MFT enumeration,
    // but that requires the full MF runtime. We use a heuristic here and refine
    // during VideoDecoder initialisation where MFT enumeration is performed.

    if (gpu.vendorId == "10DE") {       // NVIDIA
        gpu.supportsHWDecodeHEVC = true; // All Maxwell+ GPUs (2014+)
        gpu.supportsHWDecodeAV1  = true; // All Ampere+ GPUs (2020+) — may be false on older cards
    } else if (gpu.vendorId == "1002") { // AMD
        gpu.supportsHWDecodeHEVC = true; // All GCN 3+ GPUs (2015+)
        gpu.supportsHWDecodeAV1  = true; // RDNA2+ GPUs (2020+)
    } else if (gpu.vendorId == "8086") { // Intel
        gpu.supportsHWDecodeHEVC = true; // Skylake+ (2015+)
        gpu.supportsHWDecodeAV1  = true; // Xe / Arc (2021+)
    }
}

// -----------------------------------------------------------------------------
const HardwareProfile& HardwareProfiler::profile() {
    return s_profile;
}

bool HardwareProfiler::hasNVENC() {
    return s_profile.primaryGpu && s_profile.primaryGpu->supportsNVENC;
}

bool HardwareProfiler::hasHWDecodeHEVC() {
    return s_profile.primaryGpu && s_profile.primaryGpu->supportsHWDecodeHEVC;
}

bool HardwareProfiler::hasHWDecodeAV1() {
    return s_profile.primaryGpu && s_profile.primaryGpu->supportsHWDecodeAV1;
}

uint64_t HardwareProfiler::primaryVRAMBytes() {
    if (!s_profile.primaryGpu) return 0;
    return s_profile.primaryGpu->dedicatedVRAM;
}

} // namespace aura
