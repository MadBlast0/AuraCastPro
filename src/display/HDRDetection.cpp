#include "../pch.h"  // PCH
#include "HDRDetection.h"
#include "../utils/Logger.h"

HDRCapabilities HDRDetection::query(HWND hwnd, IDXGIFactory6* factory) {
    HDRCapabilities caps;
    if (!factory || !hwnd) return caps;

    // Find which monitor contains the window
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    // Enumerate DXGI outputs to find matching one
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT ai = 0; SUCCEEDED(factory->EnumAdapters1(ai, &adapter)); ai++) {
        ComPtr<IDXGIOutput> output;
        for (UINT oi = 0; SUCCEEDED(adapter->EnumOutputs(oi, &output)); oi++) {
            DXGI_OUTPUT_DESC outDesc;
            output->GetDesc(&outDesc);
            if (outDesc.Monitor != hMon) { output.Reset(); continue; }

            // Try IDXGIOutput6 for HDR info
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(output.As(&output6))) {
                DXGI_OUTPUT_DESC1 desc1;
                if (SUCCEEDED(output6->GetDesc1(&desc1))) {
                    caps.maxLuminanceNits    = desc1.MaxLuminance;
                    caps.maxFullFrameNits    = desc1.MaxFullFrameLuminance;
                    caps.minLuminanceNits    = desc1.MinLuminance;
                    caps.colorSpace          = desc1.ColorSpace;

                    caps.supportsHDR10 =
                        (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
                         desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020);
                    caps.supportsWCG =
                        (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020);

                    LOG_INFO("HDRDetection: Monitor HDR={} WCG={} maxLum={:.0f} nits",
                             caps.supportsHDR10, caps.supportsWCG, caps.maxLuminanceNits);
                }
            } else {
                LOG_INFO("HDRDetection: IDXGIOutput6 unavailable -- assuming SDR");
            }
            return caps;
        }
    }
    LOG_WARN("HDRDetection: Could not match monitor to DXGI output");
    return caps;
}

HDRCapabilities HDRDetection::requery(HWND hwnd, IDXGIFactory6* factory) {
    // Refresh factory to pick up monitor changes
    ComPtr<IDXGIFactory6> fresh;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&fresh));
    return query(hwnd, fresh ? fresh.Get() : factory);
}

bool HDRDetection::shouldUseHDRSwapchain(const HDRCapabilities& caps) {
    return caps.supportsHDR10;
}

DXGI_FORMAT HDRDetection::recommendedSwapchainFormat(const HDRCapabilities& caps) {
    return caps.supportsHDR10
        ? DXGI_FORMAT_R16G16B16A16_FLOAT
        : DXGI_FORMAT_R8G8B8A8_UNORM;
}
