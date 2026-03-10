#pragma once
// =============================================================================
// DX12Fence.h — GPU synchronisation fence.
//
// DirectX 12 uses explicit synchronisation. A fence is a counter shared
// between CPU and GPU. The GPU signals the fence when it completes a batch
// of work. The CPU waits on the fence when it needs to know the GPU is done.
//
// This class wraps the D3D12 fence with a Win32 event for efficient waiting.
//
// Common usage patterns:
//   1. CPU submits command list to GPU
//   2. CPU calls signalAndWait() to block until GPU completes
//   3. CPU then safely reads back results or reuses the command allocator
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <atomic>

using Microsoft::WRL::ComPtr;

namespace aura {

class DX12Fence {
public:
    DX12Fence(ID3D12Device* device, ID3D12CommandQueue* queue);
    ~DX12Fence();

    void init();

    // Signal the fence on the GPU side, then block CPU until GPU passes that value.
    // Returns the fence value used for this sync point.
    uint64_t signalAndWait();

    // Signal the fence on the GPU side without waiting.
    // Returns the fence value. Call waitForValue() later.
    uint64_t signal();

    // Block the CPU until the GPU fence reaches or exceeds value.
    void waitForValue(uint64_t value, uint32_t timeoutMs = 2000);

    // The last value signalled to the GPU
    uint64_t lastSignalledValue() const { return m_fenceValue.load(); }

    // The last value the GPU has completed
    uint64_t completedValue() const { return m_fence->GetCompletedValue(); }

    bool isComplete(uint64_t value) const { return completedValue() >= value; }

private:
    ID3D12Device*       m_device{};
    ID3D12CommandQueue* m_queue{};

    ComPtr<ID3D12Fence> m_fence;
    HANDLE              m_event{nullptr};

    std::atomic<uint64_t> m_fenceValue{0};
};

} // namespace aura
