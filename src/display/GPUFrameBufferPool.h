#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstdint>

using Microsoft::WRL::ComPtr;

struct FrameSlot {
    ComPtr<ID3D12Resource> nv12Texture;
    int   srvIndexY  = -1;
    int   srvIndexUV = -1;
    std::atomic<bool> inUse { false };
    int64_t timestampUs = 0;
    uint32_t width  = 0;
    uint32_t height = 0;
};

class GPUFrameBufferPool {
public:
    static constexpr int kPoolSize = 6;

    bool init(ID3D12Device* device,
              D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
              UINT srvDescriptorSize,
              uint32_t width,
              uint32_t height);

    void release();

    // Acquire a free slot (blocks up to 16ms, returns nullptr on timeout)
    FrameSlot* acquireSlot();

    // Return a slot to the pool after GPU is done with it
    void releaseSlot(FrameSlot* slot);

    // Resize all textures (e.g. after orientation change)
    bool resize(ID3D12Device* device,
                D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
                UINT srvDescriptorSize,
                uint32_t newWidth,
                uint32_t newHeight);

    uint32_t width()  const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    bool allocSlot(ID3D12Device* device,
                   D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
                   UINT srvDescSize,
                   int idx,
                   uint32_t w,
                   uint32_t h);

    std::array<FrameSlot, kPoolSize> m_slots;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    int      m_srvBaseIndex = 0;
};
