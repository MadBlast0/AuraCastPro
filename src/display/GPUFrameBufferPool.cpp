#include "../pch.h"  // PCH
#include "GPUFrameBufferPool.h"
#include "../utils/Logger.h"
#include <thread>
#include <chrono>

bool GPUFrameBufferPool::init(ID3D12Device* device,
                               D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
                               UINT srvDescriptorSize,
                               uint32_t width,
                               uint32_t height) {
    m_width  = width;
    m_height = height;
    m_srvBaseIndex = 0;

    for (int i = 0; i < kPoolSize; i++) {
        if (!allocSlot(device, srvHeapStart, srvDescriptorSize, i, width, height)) {
            AURA_LOG_ERROR("GPUFrameBufferPool", "Failed to allocate slot {}", i);
            return false;
        }
    }
    AURA_LOG_INFO("GPUFrameBufferPool", "{} NV12 slots allocated ({}x{})", kPoolSize, width, height);
    return true;
}

bool GPUFrameBufferPool::allocSlot(ID3D12Device* device,
                                    D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
                                    UINT srvDescSize,
                                    int idx,
                                    uint32_t w,
                                    uint32_t h) {
    // NV12 texture (Y plane full res, UV plane half res — DX12 handles this as a single resource)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width            = w;
    texDesc.Height           = h;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels        = 1;
    texDesc.Format           = DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr, IID_PPV_ARGS(&m_slots[idx].nv12Texture));
    if (FAILED(hr)) return false;

    m_slots[idx].width  = w;
    m_slots[idx].height = h;
    m_slots[idx].inUse  = false;

    // Y plane SRV (subresource 0)
    int srvIdx = m_srvBaseIndex + idx * 2;
    m_slots[idx].srvIndexY = srvIdx;
    D3D12_CPU_DESCRIPTOR_HANDLE hY = srvHeapStart;
    hY.ptr += (SIZE_T)(srvIdx * srvDescSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvY = {};
    srvY.Format                = DXGI_FORMAT_R8_UNORM;
    srvY.ViewDimension         = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvY.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvY.Texture2D.MipLevels   = 1;
    srvY.Texture2D.PlaneSlice  = 0;
    device->CreateShaderResourceView(m_slots[idx].nv12Texture.Get(), &srvY, hY);

    // UV plane SRV (subresource 1)
    m_slots[idx].srvIndexUV = srvIdx + 1;
    D3D12_CPU_DESCRIPTOR_HANDLE hUV = srvHeapStart;
    hUV.ptr += (SIZE_T)((srvIdx + 1) * srvDescSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvUV = {};
    srvUV.Format              = DXGI_FORMAT_R8G8_UNORM;
    srvUV.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvUV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvUV.Texture2D.MipLevels = 1;
    srvUV.Texture2D.PlaneSlice = 1;
    device->CreateShaderResourceView(m_slots[idx].nv12Texture.Get(), &srvUV, hUV);

    return true;
}

FrameSlot* GPUFrameBufferPool::acquireSlot() {
    for (int attempt = 0; attempt < 32; attempt++) {
        for (auto& slot : m_slots) {
            bool expected = false;
            if (slot.inUse.compare_exchange_strong(expected, true)) {
                return &slot;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Pool exhausted — release oldest (inUse) slot by force
    AURA_LOG_WARN("GPUFrameBufferPool", "Pool exhausted — dropping oldest frame");
    auto& oldest = m_slots[0];
    oldest.inUse = true;
    return &oldest;
}

void GPUFrameBufferPool::releaseSlot(FrameSlot* slot) {
    if (slot) slot->inUse = false;
}

bool GPUFrameBufferPool::resize(ID3D12Device* device,
                                 D3D12_CPU_DESCRIPTOR_HANDLE srvHeapStart,
                                 UINT srvDescriptorSize,
                                 uint32_t newWidth,
                                 uint32_t newHeight) {
    release();
    return init(device, srvHeapStart, srvDescriptorSize, newWidth, newHeight);
}

void GPUFrameBufferPool::release() {
    for (auto& slot : m_slots) {
        slot.nv12Texture.Reset();
        slot.inUse = false;
    }
}
