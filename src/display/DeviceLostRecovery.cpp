// =============================================================================
// DeviceLostRecovery.cpp -- D3D12 TDR / device-lost recovery
// =============================================================================
// Called after every swapchain->Present():
//
//   HRESULT hr = swapchain->Present(1, 0);
//   if (g_recovery->checkAndRecover(hr, device, swapchain)) {
//       // recovery was triggered -- current frame is discarded, next is fine
//   }
//
// Recovery sequence (Task 108 spec):
//   1. Set m_recovering flag (stops render loop from submitting more work)
//   2. pauseNetworkThreads()
//   3. Wait for all in-flight GPU work (handled inside reinitDX12)
//   4. Destroy ALL DX12 resources in reverse creation order
//   5. Re-run DX12Manager::init() chain
//   6. Clear m_recovering flag, resume
// =============================================================================
#include "../pch.h"  // PCH
#include "DeviceLostRecovery.h"
#include "../utils/Logger.h"

#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace aura {

DeviceLostRecovery::DeviceLostRecovery(DeviceLostCallbacks callbacks)
    : m_cb(std::move(callbacks))
{}

// -----------------------------------------------------------------------------
bool DeviceLostRecovery::checkAndRecover(HRESULT presentResult,
                                          ID3D12Device* device,
                                          IDXGISwapChain3* /*swapchain*/)
{
    if (presentResult != DXGI_ERROR_DEVICE_REMOVED &&
        presentResult != DXGI_ERROR_DEVICE_RESET)
    {
        return false; // Normal frame -- nothing to do
    }

    if (m_recovering.exchange(true)) {
        // Already recovering on another thread -- skip
        return true;
    }

    // Determine root cause
    HRESULT reason = DXGI_ERROR_DEVICE_REMOVED;
    if (device) {
        reason = device->GetDeviceRemovedReason();
    }

    AURA_LOG_CRITICAL("DeviceLostRecovery",
        "GPU device lost! Present HRESULT=0x{:08X}  Reason=0x{:08X} ({})",
        static_cast<uint32_t>(presentResult),
        static_cast<uint32_t>(reason),
        hresultToString(reason));

    doRecovery(reason, device);

    m_recovering.store(false);
    return true; // recovery was triggered (whether it succeeded or not)
}

// -----------------------------------------------------------------------------
bool DeviceLostRecovery::doRecovery(HRESULT removeReason, ID3D12Device* /*device*/)
{
    ++m_retryCount;
    if (m_retryCount > m_maxRetries) {
        AURA_LOG_CRITICAL("DeviceLostRecovery",
            "Max recovery attempts ({}) exceeded. GPU is unrecoverable.",
            m_maxRetries);
        return false;
    }

    AURA_LOG_WARN("DeviceLostRecovery",
        "Beginning recovery attempt {}/{} ...", m_retryCount, m_maxRetries);

    // ── Step 1: Pause all background threads that submit GPU work ────────────
    if (m_cb.pauseNetworkThreads) {
        m_cb.pauseNetworkThreads();
        AURA_LOG_INFO("DeviceLostRecovery", "Network threads paused.");
    }

    // Brief pause -- let any in-flight work drain on the GPU side
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ── Step 2: Re-create DX12 device, command queues, descriptor heaps ──────
    if (m_cb.reinitDX12) {
        if (!m_cb.reinitDX12()) {
            AURA_LOG_ERROR("DeviceLostRecovery", "reinitDX12 failed.");
            if (m_cb.resumeNetworkThreads) m_cb.resumeNetworkThreads();
            return false;
        }
        AURA_LOG_INFO("DeviceLostRecovery", "DX12 device re-created.");
    }

    // ── Step 3: Re-create GPU frame buffer pool ───────────────────────────────
    if (m_cb.reinitFrameBuffers) {
        if (!m_cb.reinitFrameBuffers()) {
            AURA_LOG_ERROR("DeviceLostRecovery", "reinitFrameBuffers failed.");
            if (m_cb.resumeNetworkThreads) m_cb.resumeNetworkThreads();
            return false;
        }
        AURA_LOG_INFO("DeviceLostRecovery", "Frame buffer pool re-created.");
    }

    // ── Step 4: Re-create swapchain back-buffers and RTVs ────────────────────
    if (m_cb.reinitSwapchain) {
        if (!m_cb.reinitSwapchain()) {
            AURA_LOG_ERROR("DeviceLostRecovery", "reinitSwapchain failed.");
            if (m_cb.resumeNetworkThreads) m_cb.resumeNetworkThreads();
            return false;
        }
        AURA_LOG_INFO("DeviceLostRecovery", "Swapchain re-created.");
    }

    // ── Step 5: Recompile root signatures and PSOs ────────────────────────────
    if (m_cb.reinitPSOs) {
        if (!m_cb.reinitPSOs()) {
            AURA_LOG_ERROR("DeviceLostRecovery", "reinitPSOs failed.");
            if (m_cb.resumeNetworkThreads) m_cb.resumeNetworkThreads();
            return false;
        }
        AURA_LOG_INFO("DeviceLostRecovery", "Root signatures and PSOs reloaded.");
    }

    // ── Step 6: Resume background threads ────────────────────────────────────
    if (m_cb.resumeNetworkThreads) {
        m_cb.resumeNetworkThreads();
        AURA_LOG_INFO("DeviceLostRecovery", "Network threads resumed.");
    }

    m_retryCount = 0; // successful recovery resets the counter
    AURA_LOG_INFO("DeviceLostRecovery",
        "GPU device recovered successfully after removal reason: {}",
        hresultToString(removeReason));
    return true;
}

// -----------------------------------------------------------------------------
std::string DeviceLostRecovery::hresultToString(HRESULT hr)
{
    switch (hr) {
    case DXGI_ERROR_DEVICE_HUNG:         return "DEVICE_HUNG (GPU timeout/TDR)";
    case DXGI_ERROR_DEVICE_REMOVED:      return "DEVICE_REMOVED (driver unloaded)";
    case DXGI_ERROR_DEVICE_RESET:        return "DEVICE_RESET (driver reset)";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DRIVER_INTERNAL_ERROR";
    case DXGI_ERROR_INVALID_CALL:        return "INVALID_CALL";
    case S_OK:                           return "S_OK";
    default: {
        std::ostringstream oss;
        oss << "0x" << std::uppercase << std::hex
            << std::setw(8) << std::setfill('0')
            << static_cast<uint32_t>(hr);
        return oss.str();
    }
    }
}

} // namespace aura
