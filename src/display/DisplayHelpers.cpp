#include "../pch.h"  // PCH
#include "DisplayHelpers.h"
#include "HDRDetection.h"
#include "../utils/Logger.h"
#include <shellscalingapi.h>
#include <sstream>

// ─── SwapchainResize ──────────────────────────────────────────────────────────

bool SwapchainResize::execute(IDXGISwapChain4*      swapchain,
                               ID3D12Device*         device,
                               ID3D12Fence*          fences[3],
                               UINT64                fenceValues[3],
                               ID3D12DescriptorHeap* rtvHeap,
                               UINT                  rtvDescSize,
                               UINT                  newWidth,
                               UINT                  newHeight,
                               Resources&            out) {
    // 1. Wait for ALL in-flight frames
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    for (int i = 0; i < 3; i++) {
        if (fences[i] && fences[i]->GetCompletedValue() < fenceValues[i]) {
            fences[i]->SetEventOnCompletion(fenceValues[i], event);
            WaitForSingleObject(event, 2000);
        }
    }
    CloseHandle(event);

    // 2. Release all back-buffer references
    for (int i = 0; i < 3; i++) out.backBuffers[i].Reset();

    // 3. Resize
    HRESULT hr = swapchain->ResizeBuffers(3, newWidth, newHeight,
                                           DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
        LOG_ERROR("SwapchainResize: ResizeBuffers failed: 0x{:X}", (uint32_t)hr);
        return false;
    }

    // 4. Re-acquire back buffers and re-create RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < 3; i++) {
        hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&out.backBuffers[i]));
        if (FAILED(hr)) {
            LOG_ERROR("SwapchainResize: GetBuffer({}) failed: 0x{:X}", i, (uint32_t)hr);
            return false;
        }
        device->CreateRenderTargetView(out.backBuffers[i].Get(), nullptr, rtvHandle);
        out.rtvHandles[i] = rtvHandle;
        rtvHandle.ptr += rtvDescSize;
    }

    out.width  = newWidth;
    out.height = newHeight;
    LOG_INFO("SwapchainResize: Resized to {}x{}", newWidth, newHeight);
    return true;
}

// Simple overload: no fence tracking. Caller must guarantee GPU idle.
bool SwapchainResize::execute(IDXGISwapChain4*       swapchain,
                               ID3D12Device*          device,
                               ID3D12DescriptorHeap*  rtvHeap,
                               UINT                   newWidth,
                               UINT                   newHeight,
                               UINT                   bufferCount) {
    HRESULT hr = swapchain->ResizeBuffers(
        bufferCount, newWidth, newHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
        LOG_ERROR("SwapchainResize: ResizeBuffers failed: 0x{:X}", (uint32_t)hr);
        return false;
    }
    // Re-create RTVs from heap start
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT descSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < bufferCount; ++i) {
        ComPtr<ID3D12Resource> buf;
        hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&buf));
        if (FAILED(hr)) {
            LOG_ERROR("SwapchainResize: GetBuffer({}) failed: 0x{:X}", i, (uint32_t)hr);
            return false;
        }
        device->CreateRenderTargetView(buf.Get(), nullptr, rtvHandle);
        rtvHandle.ptr += descSize;
    }
    LOG_INFO("SwapchainResize (simple): Resized to {}x{}", newWidth, newHeight);
    return true;
}

// ─── RotationHandler ─────────────────────────────────────────────────────────

void RotationHandler::onDimensionsChanged(uint32_t w, uint32_t h) {
    bool portrait  = h > w;
    bool changed   = (w != m_current.width || h != m_current.height);
    if (!changed) return;

    LOG_INFO("RotationHandler: {}x{} -> {}x{} ({})",
             m_current.width, m_current.height, w, h,
             portrait ? "portrait" : "landscape");

    m_current = { w, h, portrait };
    if (m_callback) m_callback(m_current);
}

// ─── MultiMonitor ────────────────────────────────────────────────────────────

static std::vector<MonitorInfo>* g_enumResult = nullptr;

BOOL CALLBACK MultiMonitor::enumProc(HMONITOR hMon, HDC, LPRECT, LPARAM) {
    if (!g_enumResult) return TRUE;
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hMon, &mi);

    MonitorInfo info;
    info.handle    = hMon;
    info.bounds    = mi.rcMonitor;
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.name      = mi.szDevice;
    info.isHDR     = false; // queried separately if needed
    info.dpi       = 96;    // default

    // Get DPI
    UINT dpiX = 96, dpiY = 96;
    if (GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK)
        info.dpi = dpiX;

    // Friendly name via display device
    DISPLAY_DEVICEW dd = {};
    dd.cb = sizeof(dd);
    if (EnumDisplayDevicesW(mi.szDevice, 0, &dd, 0)) {
        // Convert WCHAR wide string to std::string via WideCharToMultiByte
        int len = WideCharToMultiByte(CP_UTF8, 0, dd.DeviceString, -1, nullptr, 0, nullptr, nullptr);
        if (len > 1) {
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_UTF8, 0, dd.DeviceString, -1, buf.data(), len, nullptr, nullptr);
            info.friendlyName = std::string(buf.data(), len - 1);
        } else {
            info.friendlyName = "Display";
        }
    } else {
        info.friendlyName = "Display";
    }

    g_enumResult->push_back(info);
    return TRUE;
}

std::vector<MonitorInfo> MultiMonitor::enumerate() {
    std::vector<MonitorInfo> result;
    g_enumResult = &result;
    EnumDisplayMonitors(nullptr, nullptr, enumProc, 0);
    g_enumResult = nullptr;
    return result;
}

void MultiMonitor::moveWindowTo(HWND hwnd, const MonitorInfo& mon) {
    RECT& r = const_cast<RECT&>(mon.bounds);
    SetWindowPos(hwnd, nullptr, r.left, r.top,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

MonitorInfo MultiMonitor::getWindowMonitor(HWND hwnd) {
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    auto monitors = enumerate();
    for (auto& m : monitors)
        if (m.handle == hMon) return m;
    return monitors.empty() ? MonitorInfo{} : monitors[0];
}

std::string MultiMonitor::formatLabel(const MonitorInfo& mon) {
    std::ostringstream ss;
    int w = mon.bounds.right  - mon.bounds.left;
    int h = mon.bounds.bottom - mon.bounds.top;
    ss << mon.friendlyName;
    if (mon.isPrimary) ss << " (Primary)";
    ss << " " << w << "x" << h;
    if (mon.isHDR) ss << " HDR";
    return ss.str();
}
