// =============================================================================
// DX12Fence.cpp -- GPU/CPU synchronisation fence implementation
// =============================================================================

#include "../pch.h"  // PCH
#include "DX12Fence.h"
#include "../utils/Logger.h"

#include <stdexcept>
#include <format>

namespace aura {

// -----------------------------------------------------------------------------
DX12Fence::DX12Fence(ID3D12Device* device, ID3D12CommandQueue* queue)
    : m_device(device)
    , m_queue(queue) {}

// -----------------------------------------------------------------------------
DX12Fence::~DX12Fence() {
    if (m_event) {
        CloseHandle(m_event);
        m_event = nullptr;
    }
}

// -----------------------------------------------------------------------------
void DX12Fence::init() {
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) {
        throw std::runtime_error(std::format(
            "DX12Fence: CreateFence failed: {:08X}", (uint32_t)hr));
    }

    m_fenceValue.store(0);

    // Create a Win32 auto-reset event for efficient CPU waiting
    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_event) {
        throw std::runtime_error("DX12Fence: CreateEvent failed");
    }

    AURA_LOG_DEBUG("DX12Fence", "Fence initialised.");
}

// -----------------------------------------------------------------------------
uint64_t DX12Fence::signal() {
    const uint64_t signalValue = m_fenceValue.fetch_add(1) + 1;
    HRESULT hr = m_queue->Signal(m_fence.Get(), signalValue);
    if (FAILED(hr)) {
        AURA_LOG_ERROR("DX12Fence", "Signal failed: {:08X}", (uint32_t)hr);
    }
    return signalValue;
}

// -----------------------------------------------------------------------------
void DX12Fence::waitForValue(uint64_t value, uint32_t timeoutMs) {
    if (m_fence->GetCompletedValue() >= value) {
        return; // Already done -- no wait needed
    }

    // Set event to fire when fence reaches our target value
    HRESULT hr = m_fence->SetEventOnCompletion(value, m_event);
    if (FAILED(hr)) {
        AURA_LOG_ERROR("DX12Fence", "SetEventOnCompletion failed: {:08X}", (uint32_t)hr);
        return;
    }

    const DWORD result = WaitForSingleObject(m_event, timeoutMs);
    if (result == WAIT_TIMEOUT) {
        AURA_LOG_WARN("DX12Fence", "GPU fence wait timed out after {}ms (value={})",
                      timeoutMs, value);
    } else if (result != WAIT_OBJECT_0) {
        AURA_LOG_ERROR("DX12Fence", "WaitForSingleObject failed: {}", GetLastError());
    }
}

// -----------------------------------------------------------------------------
uint64_t DX12Fence::signalAndWait() {
    const uint64_t val = signal();
    waitForValue(val);
    return val;
}

} // namespace aura
