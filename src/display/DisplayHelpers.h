#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <functional>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;

// ─── Swapchain Resize ─────────────────────────────────────────────────────────

// Handles DXGI swapchain resizing safely.
// Must be called at the START of a frame, never from WM_SIZE.
class SwapchainResize {
public:
    struct Resources {
        ComPtr<ID3D12Resource> backBuffers[3];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[3];
        UINT width  = 0;
        UINT height = 0;
    };

    static bool execute(IDXGISwapChain4*       swapchain,
                        ID3D12Device*          device,
                        ID3D12Fence*           fences[3],
                        UINT64                 fenceValues[3],
                        ID3D12DescriptorHeap*  rtvHeap,
                        UINT                   rtvDescSize,
                        UINT                   newWidth,
                        UINT                   newHeight,
                        Resources&             out);
};

// ─── Rotation Handler ─────────────────────────────────────────────────────────

// Handles phone orientation changes (portrait↔landscape).
class RotationHandler {
public:
    struct FrameDimensions {
        uint32_t width;
        uint32_t height;
        bool     isPortrait;
    };

    // Call when a new SPS or stream header arrives with updated dimensions
    void onDimensionsChanged(uint32_t newWidth, uint32_t newHeight);

    FrameDimensions current() const { return m_current; }

    // Callback fires on the calling thread when orientation changes
    void setOnChanged(std::function<void(FrameDimensions)> cb) {
        m_callback = std::move(cb);
    }

private:
    FrameDimensions m_current = { 1920, 1080, false };
    std::function<void(FrameDimensions)> m_callback;
};

// ─── Multi-Monitor Support ───────────────────────────────────────────────────

struct MonitorInfo {
    std::wstring name;
    std::string  friendlyName;
    RECT         bounds;
    bool         isPrimary;
    bool         isHDR;
    UINT         dpi;
    HMONITOR     handle;
};

class MultiMonitor {
public:
    // Enumerate all active monitors
    static std::vector<MonitorInfo> enumerate();

    // Move window to specified monitor (by index from enumerate())
    static void moveWindowTo(HWND hwnd, const MonitorInfo& mon);

    // Get the monitor currently containing the majority of the window
    static MonitorInfo getWindowMonitor(HWND hwnd);

    // Format for Settings dropdown: "Display 1 (Primary) 3840×2160 HDR"
    static std::string formatLabel(const MonitorInfo& mon);

private:
    static BOOL CALLBACK enumProc(HMONITOR, HDC, LPRECT, LPARAM);
};
