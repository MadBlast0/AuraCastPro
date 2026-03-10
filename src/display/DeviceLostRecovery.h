#pragma once
// =============================================================================
// DeviceLostRecovery.h — D3D12 GPU device-lost / TDR recovery manager
// Task 108: Detects DXGI_ERROR_DEVICE_REMOVED after every Present() and
//           fully re-initialises the DX12 stack without restarting the app.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <atomic>
#include <functional>
#include <string>

namespace aura {

// Callbacks the DX12Manager sets so recovery can rebuild subsystems
struct DeviceLostCallbacks {
    std::function<bool()> reinitDX12;          // Step 1: re-create device + queues
    std::function<bool()> reinitFrameBuffers;  // Step 2: re-create frame buffer pool
    std::function<bool()> reinitSwapchain;     // Step 3: re-create swapchain + RTVs
    std::function<bool()> reinitPSOs;          // Step 4: reload root sigs + PSOs
    std::function<void()> pauseNetworkThreads; // before teardown
    std::function<void()> resumeNetworkThreads;// after recovery
};

class DeviceLostRecovery {
public:
    explicit DeviceLostRecovery(DeviceLostCallbacks callbacks);
    ~DeviceLostRecovery() = default;

    // Call after every swapchain->Present(). Returns true if recovery was needed.
    bool checkAndRecover(HRESULT presentResult,
                         ID3D12Device* device,
                         IDXGISwapChain3* swapchain);

    // True while recovery is in progress — render loop must pause
    bool isRecovering() const { return m_recovering.load(); }

    // Maximum consecutive recovery attempts before giving up
    void setMaxRetries(int n) { m_maxRetries = n; }
    int  retryCount()   const { return m_retryCount; }

private:
    bool        doRecovery(HRESULT removeReason, ID3D12Device* device);
    std::string hresultToString(HRESULT hr);

    DeviceLostCallbacks m_cb;
    std::atomic<bool>   m_recovering{false};
    int                 m_retryCount{0};
    int                 m_maxRetries{3};
};

} // namespace aura
