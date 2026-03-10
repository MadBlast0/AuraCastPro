#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <functional>

using Microsoft::WRL::ComPtr;

struct HDRCapabilities {
    bool  supportsHDR10        = false;
    bool  supportsWCG          = false;
    float maxLuminanceNits     = 0.0f;
    float maxFullFrameNits     = 0.0f;
    float minLuminanceNits     = 0.0f;
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
};

class HDRDetection {
public:
    // Query capabilities for the monitor containing the given window.
    static HDRCapabilities query(HWND hwnd, IDXGIFactory6* factory);

    // Should be called again on WM_DISPLAYCHANGE
    static HDRCapabilities requery(HWND hwnd, IDXGIFactory6* factory);

    // Returns true if swapchain should use R16G16B16A16_FLOAT + HDR colour space
    static bool shouldUseHDRSwapchain(const HDRCapabilities& caps);

    // Recommended DXGI format for swapchain given HDR caps
    static DXGI_FORMAT recommendedSwapchainFormat(const HDRCapabilities& caps);
};
