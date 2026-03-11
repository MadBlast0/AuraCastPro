// =============================================================================
// DX12CommandQueue.cpp — Task 105: DX12 command queue with triple-buffering
//
// Manages:
//   • One D3D12_COMMAND_LIST_TYPE_DIRECT queue (render + present)
//   • One D3D12_COMMAND_LIST_TYPE_COPY queue (NV12 texture upload)
//   • Triple-buffered command allocator rotation (frame N uses allocator N%3)
//   • GPU timestamp query heap (2 timestamps per pass × 8 passes × 3 frames)
//     → feeds PerformanceOverlay with per-pass GPU time
//   • PIX/RenderDoc profiling markers (stripped in Release)
//
// Frame lifecycle:
//   beginFrame(frameIndex)   → reset allocator[frameIndex%3], reset cmdList
//   executeCommandList(list) → GPU executes all recorded draw calls
//   signalAndPresent(fence)  → signal fence[frameIndex%3] + swapchain Present
// =============================================================================
#include "../pch.h"  // PCH
#include "DX12CommandQueue.h"
#include "DX12Fence.h"
#include "GPUProfileMarkers.h"
#include "../utils/Logger.h"

#include <stdexcept>
#include <format>

namespace aura {

DX12CommandQueue::DX12CommandQueue(ID3D12Device* device) : m_device(device) {}
DX12CommandQueue::~DX12CommandQueue() { shutdown(); }

// ── init ──────────────────────────────────────────────────────────────────────

void DX12CommandQueue::init() {
    // ── Direct command queue (rendering + present) ───────────────────────────
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        HRESULT hr = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_directQueue));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateCommandQueue(Direct): 0x{:08X}", (uint32_t)hr));
    }

    // ── Copy queue (async NV12 texture upload from decoder) ──────────────────
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type     = D3D12_COMMAND_LIST_TYPE_COPY;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        HRESULT hr = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_copyQueue));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateCommandQueue(Copy): 0x{:08X}", (uint32_t)hr));
    }

    // ── Triple-buffered command allocators (one per in-flight frame) ─────────
    m_directAllocators.resize(kFrameCount);
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_directAllocators[i]));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateCommandAllocator[{}]: 0x{:08X}", i, (uint32_t)hr));
        m_directAllocators[i]->SetName(
            std::wstring(L"DirectAllocator[" + std::to_wstring(i) + L"]").c_str());
    }

    // ── Copy queue command allocator (single, reset per upload) ─────────────
    {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COPY,
            IID_PPV_ARGS(&m_copyAllocator));
        if (FAILED(hr))
            throw std::runtime_error(std::format("CreateCommandAllocator(Copy): 0x{:08X}", (uint32_t)hr));
    }

    // ── GPU timestamp query heap ──────────────────────────────────────────────
    // 2 timestamps (begin + end) × kMaxPasses × kFrameCount
    {
        D3D12_QUERY_HEAP_DESC qhDesc{};
        qhDesc.Type     = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        qhDesc.Count    = kMaxPasses * 2 * kFrameCount;
        qhDesc.NodeMask = 0;
        HRESULT hr = m_device->CreateQueryHeap(&qhDesc, IID_PPV_ARGS(&m_timestampHeap));
        if (FAILED(hr)) {
            AURA_LOG_WARN("DX12CommandQueue",
                "CreateQueryHeap (timestamps) failed 0x{:08X} — GPU timing disabled.", (uint32_t)hr);
            // Not fatal — just disable GPU timing
        } else {
            // Readback buffer: one UINT64 per query slot
            D3D12_HEAP_PROPERTIES hp{};
            hp.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(
                sizeof(uint64_t) * kMaxPasses * 2 * kFrameCount);
            HRESULT hrTS = m_device->CreateCommittedResource(
                &hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&m_timestampReadback));
            if (FAILED(hrTS)) {
                AURA_LOG_WARN("DX12CommandQueue",
                    "CreateCommittedResource for timestamp readback failed: 0x{:08X}. "
                    "GPU timing disabled.", (unsigned)hrTS);
                m_gpuTimingEnabled = false;
            }
        }
    }

    // ── Get GPU timestamp frequency for ms conversion ────────────────────────
    if (FAILED(m_directQueue->GetTimestampFrequency(&m_gpuTimestampFreq))) {
        AURA_LOG_WARN("DX12CommandQueue",
            "GetTimestampFrequency failed — GPU timing metrics will be zero.");
        m_gpuTimestampFreq = 1;  // prevent divide-by-zero in timing calculations
    }

    AURA_LOG_INFO("DX12CommandQueue",
        "Initialised: {} frame allocators, GPU timestamp freq {}MHz.",
        kFrameCount, m_gpuTimestampFreq / 1'000'000ULL);
}

// ── shutdown ──────────────────────────────────────────────────────────────────

void DX12CommandQueue::shutdown() {
    m_timestampReadback.Reset();
    m_timestampHeap.Reset();
    m_copyAllocator.Reset();
    m_directAllocators.clear();
    m_copyQueue.Reset();
    m_directQueue.Reset();
}

// ── per-frame begin/execute/present ──────────────────────────────────────────

void DX12CommandQueue::beginFrame(uint32_t frameIndex) {
    // Rotate to the allocator for this frame (triple-buffer: 0, 1, 2, 0, 1, 2...)
    const uint32_t slot = frameIndex % kFrameCount;
    HRESULT hr = m_directAllocators[slot]->Reset();
    if (FAILED(hr))
        AURA_LOG_WARN("DX12CommandQueue",
            "CommandAllocator[{}]->Reset() 0x{:08X}", slot, (uint32_t)hr);
    m_currentFrameSlot = slot;
}

void DX12CommandQueue::executeCommandList(ID3D12GraphicsCommandList* list) {
    AURA_GPU_MARKER(list, aura::GPU::ColourPresent, "ExecuteCommandList");
    ID3D12CommandList* lists[] = { list };
    m_directQueue->ExecuteCommandLists(1, lists);
}

void DX12CommandQueue::createCommandList(
        ID3D12GraphicsCommandList** outList,
        ID3D12CommandAllocator**    outAllocator) {

    const uint32_t slot = m_currentFrameSlot;
    *outAllocator = m_directAllocators[slot].Get();
    (*outAllocator)->Reset();

    HRESULT hr = m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        *outAllocator, nullptr, IID_PPV_ARGS(outList));
    if (FAILED(hr))
        throw std::runtime_error(std::format("CreateCommandList: 0x{:08X}", (uint32_t)hr));
}

// ── copy queue helpers ────────────────────────────────────────────────────────

void DX12CommandQueue::executeCopyCommandList(ID3D12GraphicsCommandList* list) {
    ID3D12CommandList* lists[] = { list };
    m_copyQueue->ExecuteCommandLists(1, lists);
}

void DX12CommandQueue::createCopyCommandList(
        ID3D12GraphicsCommandList** outList) {
    m_copyAllocator->Reset();
    HRESULT hr = m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_COPY,
        m_copyAllocator.Get(), nullptr, IID_PPV_ARGS(outList));
    if (FAILED(hr))
        throw std::runtime_error(std::format("CreateCommandList(Copy): 0x{:08X}", (uint32_t)hr));
}

// ── GPU timestamp helpers ─────────────────────────────────────────────────────

void DX12CommandQueue::beginTimestamp(ID3D12GraphicsCommandList* list, uint32_t passIndex) {
    if (!m_timestampHeap) return;
    const uint32_t queryIdx = (m_currentFrameSlot * kMaxPasses + passIndex) * 2;
    list->EndQuery(m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIdx);
}

void DX12CommandQueue::endTimestamp(ID3D12GraphicsCommandList* list, uint32_t passIndex) {
    if (!m_timestampHeap) return;
    const uint32_t queryIdx = (m_currentFrameSlot * kMaxPasses + passIndex) * 2 + 1;
    list->EndQuery(m_timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryIdx);
}

double DX12CommandQueue::readPassTimeMs(uint32_t frameSlot, uint32_t passIndex) const {
    if (!m_timestampReadback || m_gpuTimestampFreq == 0) return 0.0;

    const uint32_t baseIdx = (frameSlot * kMaxPasses + passIndex) * 2;
    const UINT64   byteOff = baseIdx * sizeof(uint64_t);
    const UINT64   byteEnd = byteOff + 2 * sizeof(uint64_t);

    D3D12_RANGE range{ byteOff, byteEnd };
    void* mapped = nullptr;
    if (FAILED(m_timestampReadback->Map(0, &range, &mapped))) return 0.0;

    const auto* ts = reinterpret_cast<const uint64_t*>(
        static_cast<const uint8_t*>(mapped) + byteOff);
    const double ms = (ts[1] > ts[0])
        ? (static_cast<double>(ts[1] - ts[0]) / m_gpuTimestampFreq) * 1000.0
        : 0.0;

    D3D12_RANGE emptyRange{};
    m_timestampReadback->Unmap(0, &emptyRange);
    return ms;
}

} // namespace aura
