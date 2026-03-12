#pragma once
// =============================================================================
// DX12DeviceContext.h — Task 103: GPU resource allocator
//
// Manages descriptor heaps and provides helpers for creating GPU resources:
//   • RTV heap  (32 slots)  — swapchain back buffers + intermediate RTs
//   • SRV heap (256 slots)  — NV12 Y/UV planes, intermediate textures, etc.
//   • allocateTexture2D()   — creates a GPU texture resource (VRAM)
//   • allocateUploadBuffer() — creates a CPU-visible upload heap buffer
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

using Microsoft::WRL::ComPtr;

namespace aura {

class DX12DeviceContext {
public:
    explicit DX12DeviceContext(ID3D12Device* device);
    ~DX12DeviceContext();

    void init();
    void shutdown();

    // ── Descriptor allocation ─────────────────────────────────────────────────
    D3D12_CPU_DESCRIPTOR_HANDLE allocateRTV();
    D3D12_CPU_DESCRIPTOR_HANDLE allocateSRV_CPU();
    D3D12_GPU_DESCRIPTOR_HANDLE allocateSRV_GPU();

    ID3D12DescriptorHeap* srvHeap() const { return m_srvHeap.Get(); }
    ID3D12DescriptorHeap* rtvHeap() const { return m_rtvHeap.Get(); }

    uint32_t srvDescSize() const { return m_srvDescSize; }
    uint32_t rtvDescSize() const { return m_rtvDescSize; }

    // ── Resource creation helpers (Task 103) ──────────────────────────────────

    // Create a GPU-only texture (D3D12_HEAP_TYPE_DEFAULT).
    // Returns a committed resource in the given initial state.
    // Call with D3D12_RESOURCE_STATE_COMMON for NV12 frame buffers.
    ComPtr<ID3D12Resource> allocateTexture2D(
        uint32_t              width,
        uint32_t              height,
        DXGI_FORMAT           format,
        D3D12_RESOURCE_FLAGS  flags        = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        const std::wstring&   debugName    = L"");

    // Create a CPU-visible upload heap buffer (D3D12_HEAP_TYPE_UPLOAD).
    // Used for staging before copying to a DEFAULT heap texture.
    ComPtr<ID3D12Resource> allocateUploadBuffer(
        uint64_t            byteSize,
        const std::wstring& debugName = L"");

    // Create a CPU-visible readback buffer (D3D12_HEAP_TYPE_READBACK).
    // Used for GPU → CPU downloads (screenshot, vcam readback).
    ComPtr<ID3D12Resource> allocateReadbackBuffer(
        uint64_t            byteSize,
        const std::wstring& debugName = L"");

private:
    ID3D12Device* m_device{};

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    uint32_t m_rtvDescSize{};
    uint32_t m_srvDescSize{};
    uint32_t m_rtvAllocated{};
    uint32_t m_srvAllocated{};

    static constexpr uint32_t kMaxRTVs = 32;
    static constexpr uint32_t kMaxSRVs = 256;
};

} // namespace aura
