#pragma once
// =============================================================================
// DX12CommandQueue.h — Task 105: Triple-buffered command queue manager
//
// Owns:
//   • Direct queue (rendering + present)
//   • Copy queue (NV12 texture uploads from decoder thread)
//   • kFrameCount (3) command allocators — rotated by frameIndex % 3
//   • GPU timestamp query heap + readback buffer for per-pass timing
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace aura {

class DX12CommandQueue {
public:
    static constexpr uint32_t kFrameCount = 3;   // triple-buffering
    static constexpr uint32_t kMaxPasses  = 8;   // NV12→RGB, Scale, Tonemap, Pacing…

    explicit DX12CommandQueue(ID3D12Device* device);
    ~DX12CommandQueue();

    void init();
    void shutdown();

    // ── Frame lifecycle ───────────────────────────────────────────────────────
    // Call once per frame before recording commands. Resets the correct allocator.
    void beginFrame(uint32_t frameIndex);

    // Submit a closed Direct command list for GPU execution
    void executeCommandList(ID3D12GraphicsCommandList* list);

    // Create a new Direct command list backed by the current frame's allocator
    void createCommandList(ID3D12GraphicsCommandList** outList,
                           ID3D12CommandAllocator**    outAllocator);

    // ── Copy queue (async texture upload) ────────────────────────────────────
    void executeCopyCommandList(ID3D12GraphicsCommandList* list);
    void createCopyCommandList(ID3D12GraphicsCommandList** outList);

    // ── GPU timestamp queries (per-pass timing for PerformanceOverlay) ───────
    void   beginTimestamp(ID3D12GraphicsCommandList* list, uint32_t passIndex);
    void   endTimestamp  (ID3D12GraphicsCommandList* list, uint32_t passIndex);
    // Read cached timestamp for a previous frame slot (call AFTER fence wait)
    double readPassTimeMs(uint32_t frameSlot, uint32_t passIndex) const;

    // ── Accessors ─────────────────────────────────────────────────────────────
    ID3D12CommandQueue* queue()     const { return m_directQueue.Get(); }
    ID3D12CommandQueue* copyQueue() const { return m_copyQueue.Get();   }
    uint32_t currentFrameSlot()     const { return m_currentFrameSlot;  }

private:
    ID3D12Device*                               m_device{};
    ComPtr<ID3D12CommandQueue>                  m_directQueue;
    ComPtr<ID3D12CommandQueue>                  m_copyQueue;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_directAllocators;
    ComPtr<ID3D12CommandAllocator>              m_copyAllocator;
    ComPtr<ID3D12QueryHeap>                     m_timestampHeap;
    mutable ComPtr<ID3D12Resource>              m_timestampReadback;

    uint64_t m_gpuTimestampFreq{1};
    bool     m_gpuTimingEnabled{true};
    uint32_t m_currentFrameSlot{0};
};

} // namespace aura
