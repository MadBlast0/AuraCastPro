// =============================================================================
// DX12DeviceContext.cpp -- Task 103: GPU descriptor heap manager + resource allocator
// =============================================================================
#include "../pch.h"  // PCH
#include "DX12DeviceContext.h"
#include "../utils/Logger.h"
#include <stdexcept>
#include <format>

namespace aura {

DX12DeviceContext::DX12DeviceContext(ID3D12Device* device) : m_device(device) {}
DX12DeviceContext::~DX12DeviceContext() { shutdown(); }

// ── init ──────────────────────────────────────────────────────────────────────

void DX12DeviceContext::init() {
    // Cache descriptor sizes (hardware-specific values)
    m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_srvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTV heap -- non-shader-visible (CPU-only handles for render target views)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kMaxRTVs;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateDescriptorHeap(RTV): 0x{:08X}", (uint32_t)hr));
        (void)m_rtvHeap->SetName(L"AuraCastPro RTV Heap");
    }

    // SRV/CBV/UAV heap -- shader-visible (GPU-accessible, bound via SetDescriptorHeaps)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kMaxSRVs;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateDescriptorHeap(SRV): 0x{:08X}", (uint32_t)hr));
        (void)m_srvHeap->SetName(L"AuraCastPro SRV Heap");
    }

    AURA_LOG_INFO("DX12DeviceContext",
        "Descriptor heaps created: RTV={} slots, SRV={} slots. "
        "RTV stride={} bytes, SRV stride={} bytes.",
        kMaxRTVs, kMaxSRVs, m_rtvDescSize, m_srvDescSize);
}

void DX12DeviceContext::shutdown() {
    m_rtvHeap.Reset();
    m_srvHeap.Reset();
}

// ── descriptor allocation ─────────────────────────────────────────────────────

D3D12_CPU_DESCRIPTOR_HANDLE DX12DeviceContext::allocateRTV() {
    if (m_rtvAllocated >= kMaxRTVs)
        throw std::runtime_error("DX12DeviceContext: RTV heap exhausted");
    auto handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_rtvAllocated++) * m_rtvDescSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DeviceContext::allocateSRV_CPU() {
    if (m_srvAllocated >= kMaxSRVs)
        throw std::runtime_error("DX12DeviceContext: SRV heap exhausted");
    auto handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_srvAllocated) * m_srvDescSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12DeviceContext::allocateSRV_GPU() {
    if (m_srvAllocated >= kMaxSRVs)
        throw std::runtime_error("DX12DeviceContext: SRV heap exhausted");
    auto handle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_srvAllocated++) * m_srvDescSize;
    return handle;
}

// ── resource creation helpers ─────────────────────────────────────────────────

ComPtr<ID3D12Resource> DX12DeviceContext::allocateTexture2D(
        uint32_t              width,
        uint32_t              height,
        DXGI_FORMAT           format,
        D3D12_RESOURCE_FLAGS  flags,
        D3D12_RESOURCE_STATES initialState,
        const std::wstring&   debugName) {

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width            = width;
    rd.Height           = height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels        = 1;
    rd.Format           = format;
    rd.SampleDesc.Count = 1;
    rd.Flags            = flags;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE,
        &rd, initialState,
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr))
        throw std::runtime_error(std::format(
            "allocateTexture2D({}x{} fmt={:d}): 0x{:08X}",
            width, height, (int)format, (uint32_t)hr));

    if (!debugName.empty())
        (void)resource->SetName(debugName.c_str());

    return resource;
}

ComPtr<ID3D12Resource> DX12DeviceContext::allocateUploadBuffer(
        uint64_t            byteSize,
        const std::wstring& debugName) {

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE,
        &rd, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr))
        throw std::runtime_error(std::format(
            "allocateUploadBuffer({} bytes): 0x{:08X}", byteSize, (uint32_t)hr));

    if (!debugName.empty())
        (void)resource->SetName(debugName.c_str());

    return resource;
}

ComPtr<ID3D12Resource> DX12DeviceContext::allocateReadbackBuffer(
        uint64_t            byteSize,
        const std::wstring& debugName) {

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = m_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE,
        &rd, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr))
        throw std::runtime_error(std::format(
            "allocateReadbackBuffer({} bytes): 0x{:08X}", byteSize, (uint32_t)hr));

    if (!debugName.empty())
        (void)resource->SetName(debugName.c_str());

    return resource;
}

} // namespace aura
