// =============================================================================
// MirrorWindow.cpp — Mirror window: Win32 HWND + DX12 swapchain render loop
//
// Render pipeline per frame:
//   1. Bind NV12 decoded texture as SRV
//   2. Execute nv12_to_rgb PSO (NV12 → RGBA8 back buffer)
//   3. swapChain->Present(1, 0)        ← vsync-locked, tear-free
//   4. Signal fence, advance buffer index
// =============================================================================
#include "../pch.h"  // PCH
#include "MirrorWindow.h"
#include "MirrorWindowWin32.h"
#include "DX12Manager.h"
#include "DX12CommandQueue.h"
#include "DX12Fence.h"
#include "../integration/VCamBridge.h"
#include "../manager/PerformanceOverlay.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>
#include <format>
#include <array>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace aura {

static constexpr UINT kFrameCount = 2; // double-buffered swapchain

// Internal render state — hidden behind a pimpl to keep the header clean
struct MirrorWindow::RenderState {
    ComPtr<IDXGISwapChain4>       swapChain;
    UINT                          frameIndex{0};

    // Per-frame resources
    std::array<ComPtr<ID3D12Resource>,        kFrameCount> renderTargets;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> cmdAllocators;
    ComPtr<ID3D12GraphicsCommandList>         cmdList;

    // RTV descriptor heap
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT                         rtvDescSize{0};

    // SRV heap: [0] = NV12 Y plane, [1] = NV12 UV plane
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    UINT                         srvDescSize{0};

    // CBV upload buffer — holds NV12Constants (16 bytes) for the pixel shader
    ComPtr<ID3D12Resource>       cbvUploadBuffer;

    // Per-frame fence values for CPU/GPU sync
    std::array<uint64_t, kFrameCount> fenceValues{};
};

// -----------------------------------------------------------------------------
MirrorWindow::MirrorWindow(DX12Manager* dx12, VCamBridge* vcam)
    : m_dx12(dx12)
    , m_vcam(vcam)
    , m_win32(std::make_unique<MirrorWindowWin32>())
    , m_render(std::make_unique<RenderState>()) {}

MirrorWindow::~MirrorWindow() { shutdown(); }

// -----------------------------------------------------------------------------
void MirrorWindow::init() {
    m_win32->init();
    AURA_LOG_INFO("MirrorWindow", "Initialised.");
}

// -----------------------------------------------------------------------------
void MirrorWindow::start() {
    // Create the Win32 window
    m_win32->create(m_width, m_height, "AuraCastPro — Mirror");

    // Wire close event → application exit
    m_win32->setCloseCallback([]() {
        PostQuitMessage(0);
    });
    // Wire resize event → swapchain resize
    m_win32->setResizeCallback([this](uint32_t w, uint32_t h) {
        onResize(w, h);
    });
    // Wire key events → fullscreen toggle / overlay
    m_win32->setKeyCallback([this](uint32_t vk) {
        if (vk == 'F' || vk == VK_F11) m_win32->toggleFullscreen();
        if (vk == VK_TAB && m_overlay)  m_overlay->toggle();
    });

    createSwapchainAndResources();

    m_win32->show();
    m_running = true;

    AURA_LOG_INFO("MirrorWindow",
        "Mirror window started: {}×{} (DX12 swapchain, {} buffers).",
        m_width, m_height, kFrameCount);
}

// -----------------------------------------------------------------------------
void MirrorWindow::createSwapchainAndResources() {
    if (!m_dx12 || !m_dx12->commandQueue()) {
        AURA_LOG_WARN("MirrorWindow",
            "DX12Manager not ready — mirror window will be blank.");
        return;
    }

    ID3D12Device*       device = m_dx12->device();
    ID3D12CommandQueue* queue  = m_dx12->commandQueue()->queue();

    // ── 1. Create DXGI swap chain ─────────────────────────────────────────
    m_render->swapChain.Attach(
        m_win32->createSwapChain(queue, kFrameCount));

    if (!m_render->swapChain) {
        AURA_LOG_ERROR("MirrorWindow", "Swapchain creation failed.");
        return;
    }

    m_render->frameIndex =
        m_render->swapChain->GetCurrentBackBufferIndex();

    // ── 2. RTV descriptor heap ────────────────────────────────────────────
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = kFrameCount;
    rtvDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(
        &rtvDesc, IID_PPV_ARGS(&m_render->rtvHeap));
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindow",
            "CreateDescriptorHeap(RTV) failed: {:08X}", (uint32_t)hr);
        return;
    }
    m_render->rtvDescSize =
        device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // ── 3. SRV heap (NV12 Y + UV planes) ────────────────────────────────
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 2; // Y plane + UV plane
    srvDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(
        &srvDesc, IID_PPV_ARGS(&m_render->srvHeap));
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindow",
            "CreateDescriptorHeap(SRV) failed: {:08X}", (uint32_t)hr);
        return;
    }
    m_render->srvDescSize =
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ── 3b. CBV upload buffer (16 bytes — NV12 shader constants) ────────
    {
        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC cbDesc{};
        cbDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        cbDesc.Width            = 256; // minimum CBV alignment
        cbDesc.Height           = 1;
        cbDesc.DepthOrArraySize = 1;
        cbDesc.MipLevels        = 1;
        cbDesc.Format           = DXGI_FORMAT_UNKNOWN;
        cbDesc.SampleDesc.Count = 1;
        cbDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_render->cbvUploadBuffer));
        if (FAILED(hr)) {
            AURA_LOG_WARN("MirrorWindow",
                "CBV upload buffer creation failed ({:08X}) — shader constants will be zero.",
                (uint32_t)hr);
        }
    }

    // ── 4. Per-frame render targets + command allocators ─────────────────
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        m_render->rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kFrameCount; ++i) {
        hr = m_render->swapChain->GetBuffer(
            i, IID_PPV_ARGS(&m_render->renderTargets[i]));
        if (FAILED(hr)) {
            AURA_LOG_ERROR("MirrorWindow",
                "GetBuffer({}) failed: {:08X}", i, (uint32_t)hr);
            return;
        }
        device->CreateRenderTargetView(
            m_render->renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_render->rtvDescSize;

        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_render->cmdAllocators[i]));
        if (FAILED(hr)) {
            AURA_LOG_ERROR("MirrorWindow",
                "CreateCommandAllocator({}) failed: {:08X}", i, (uint32_t)hr);
            return;
        }
        m_render->fenceValues[i] = 0;
    }

    // ── 5. Command list (reused every frame) ─────────────────────────────
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_render->cmdAllocators[0].Get(),
        m_dx12->nv12ToRgbPSO(),   // initial PSO
        IID_PPV_ARGS(&m_render->cmdList));
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindow",
            "CreateCommandList failed: {:08X}", (uint32_t)hr);
        return;
    }
    // Command list starts open; close it so the first present can reset it
    m_render->cmdList->Close();

    AURA_LOG_INFO("MirrorWindow",
        "DX12 render resources created: {} RTVs, swapchain {}×{}.",
        kFrameCount, m_width, m_height);
}

// -----------------------------------------------------------------------------
// presentFrame() — called from the decoder callback (video thread)
// Records a GPU command list and calls swapChain->Present().
// -----------------------------------------------------------------------------
void MirrorWindow::presentFrame(uint32_t width, uint32_t height) {
    if (!m_running) return;
    if (!m_render->swapChain) return;
    if (!m_dx12 || !m_dx12->commandQueue() || !m_dx12->fence()) return;

    m_width  = width;
    m_height = height;

    const UINT fi = m_render->frameIndex;

    ID3D12Device*              device = m_dx12->device();
    ID3D12CommandQueue*        queue  = m_dx12->commandQueue()->queue();
    ID3D12GraphicsCommandList* cmd    = m_render->cmdList.Get();
    ID3D12Resource*            rt     = m_render->renderTargets[fi].Get();

    // ── CPU/GPU sync: wait until the GPU is done with this buffer ────────
    // (avoids writing to a render target still being read by the GPU)
    m_dx12->fence()->waitForValue(m_render->fenceValues[fi]);

    // ── Reset allocator + command list ───────────────────────────────────
    m_render->cmdAllocators[fi]->Reset();
    cmd->Reset(m_render->cmdAllocators[fi].Get(), m_dx12->nv12ToRgbPSO());

    // ── Transition RT: PRESENT → RENDER_TARGET ───────────────────────────
    D3D12_RESOURCE_BARRIER barrierToRT{};
    barrierToRT.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToRT.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToRT.Transition.pResource   = rt;
    barrierToRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrierToRT.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierToRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrierToRT);

    // ── Set up viewport and scissor ───────────────────────────────────────
    D3D12_VIEWPORT vp{};
    vp.Width    = static_cast<float>(m_width);
    vp.Height   = static_cast<float>(m_height);
    vp.MaxDepth = 1.0f;
    D3D12_RECT scissor{ 0, 0, (LONG)m_width, (LONG)m_height };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &scissor);

    // ── Bind render target ────────────────────────────────────────────────
    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        m_render->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(fi) * m_render->rtvDescSize;

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // ── Bind NV12 texture SRVs and draw ──────────────────────────────────
    if (m_currentTexture && m_dx12->renderRootSignature() &&
        m_dx12->nv12ToRgbPSO() && m_render->srvHeap)
    {
        cmd->SetGraphicsRootSignature(m_dx12->renderRootSignature());
        cmd->SetPipelineState(m_dx12->nv12ToRgbPSO());

        ID3D12DescriptorHeap* heaps[] = { m_render->srvHeap.Get() };
        cmd->SetDescriptorHeaps(1, heaps);

        // Bind cbuffer Constants (b0): TexelSize + Brightness/Contrast
        // Upload constants to the per-frame upload buffer (16 bytes)
        if (m_render->cbvUploadBuffer) {
            struct NV12Constants {
                float texelSizeX, texelSizeY;
                float brightness, contrast;
            };
            NV12Constants cb{};
            if (m_currentTextureWidth > 0 && m_currentTextureHeight > 0) {
                cb.texelSizeX = 1.0f / static_cast<float>(m_currentTextureWidth);
                cb.texelSizeY = 1.0f / static_cast<float>(m_currentTextureHeight);
            }
            cb.brightness = 0.0f;   // neutral: shader adds this offset
            cb.contrast   = 1.0f;   // neutral: shader multiplies by this

            void* mapped = nullptr;
            D3D12_RANGE readRange{ 0, 0 };
            if (SUCCEEDED(m_render->cbvUploadBuffer->Map(0, &readRange, &mapped))) {
                memcpy(mapped, &cb, sizeof(cb));
                m_render->cbvUploadBuffer->Unmap(0, nullptr);
            }
            cmd->SetGraphicsRootConstantBufferView(
                0, m_render->cbvUploadBuffer->GetGPUVirtualAddress());
        }

        // SRV for NV12 Y plane (format R8_UNORM)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvY{};
        srvY.Format                  = DXGI_FORMAT_R8_UNORM;
        srvY.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvY.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvY.Texture2D.MipLevels     = 1;
        srvY.Texture2D.PlaneSlice    = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle =
            m_render->srvHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateShaderResourceView(m_currentTexture.Get(), &srvY, srvHandle);
        srvHandle.ptr += m_render->srvDescSize;

        // SRV for NV12 UV plane (format R8G8_UNORM)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvUV = srvY;
        srvUV.Format              = DXGI_FORMAT_R8G8_UNORM;
        srvUV.Texture2D.PlaneSlice = 1;
        device->CreateShaderResourceView(m_currentTexture.Get(), &srvUV, srvHandle);

        // Bind SRV table at root parameter 1
        cmd->SetGraphicsRootDescriptorTable(
            1, m_render->srvHeap->GetGPUDescriptorHandleForHeapStart());

        // Full-screen triangle (vertex-less — VS generates from SV_VertexID)
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->DrawInstanced(3, 1, 0, 0);
    }

    // ── Transition RT: RENDER_TARGET → PRESENT ───────────────────────────
    D3D12_RESOURCE_BARRIER barrierToPresent{};
    barrierToPresent.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToPresent.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToPresent.Transition.pResource   = rt;
    barrierToPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrierToPresent.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrierToPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrierToPresent);

    // ── Submit + present ──────────────────────────────────────────────────
    cmd->Close();
    ID3D12CommandList* lists[] = { cmd };
    queue->ExecuteCommandLists(1, lists);

    // vsync-on present (SyncInterval=1 locks to display refresh rate)
    m_render->swapChain->Present(1, 0);

    // Signal fence so we know when the GPU finishes this frame
    m_render->fenceValues[fi] = m_dx12->fence()->signal();
    m_render->frameIndex      = m_render->swapChain->GetCurrentBackBufferIndex();
}

// -----------------------------------------------------------------------------
void MirrorWindow::setCurrentTexture(ID3D12Resource* texture) {
    m_currentTexture = texture;
    // Cache texture dimensions for the CBV constants (texel size calculation)
    if (texture) {
        D3D12_RESOURCE_DESC desc = texture->GetDesc();
        m_currentTextureWidth  = static_cast<UINT>(desc.Width);
        m_currentTextureHeight = static_cast<UINT>(desc.Height);
    }
}

// -----------------------------------------------------------------------------
void MirrorWindow::onResize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if (w == m_width && h == m_height) return;

    m_width  = w;
    m_height = h;

    if (!m_render->swapChain || !m_dx12) return;

    // Wait for GPU to finish before resizing
    m_dx12->waitForGPUIdle();

    // Release old render targets
    for (UINT i = 0; i < kFrameCount; ++i)
        m_render->renderTargets[i].Reset();

    // Resize buffers
    HRESULT hr = m_render->swapChain->ResizeBuffers(
        kFrameCount, w, h,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindow",
            "ResizeBuffers failed: {:08X}", (uint32_t)hr);
        return;
    }

    m_render->frameIndex = m_render->swapChain->GetCurrentBackBufferIndex();

    // Re-create RTVs for new buffer size
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        m_render->rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i) {
        m_render->swapChain->GetBuffer(
            i, IID_PPV_ARGS(&m_render->renderTargets[i]));
        m_dx12->device()->CreateRenderTargetView(
            m_render->renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_render->rtvDescSize;
        m_render->fenceValues[i] = 0;
    }

    AURA_LOG_INFO("MirrorWindow", "Swapchain resized to {}×{}", w, h);
}

// -----------------------------------------------------------------------------
void MirrorWindow::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_dx12) m_dx12->waitForGPUIdle();
    m_win32->hide();
}

void MirrorWindow::shutdown() {
    stop();
    // Release DX12 resources before device goes away
    if (m_render) {
        m_render->cmdList.Reset();
        for (auto& a : m_render->cmdAllocators) a.Reset();
        for (auto& rt : m_render->renderTargets) rt.Reset();
        m_render->rtvHeap.Reset();
        m_render->cbvUploadBuffer.Reset();
    m_render->srvHeap.Reset();
        m_currentTexture.Reset();
        m_render->swapChain.Reset();
    }
    m_win32->shutdown();
    AURA_LOG_INFO("MirrorWindow", "Shutdown complete.");
}

// -----------------------------------------------------------------------------
void MirrorWindow::setTitle(const std::string& title) {
    if (m_win32 && m_win32->hwnd()) {
        SetWindowTextA(m_win32->hwnd(), title.c_str());
    }
}

void MirrorWindow::setFullscreen(bool fs) { m_win32->setFullscreen(fs); }
void MirrorWindow::toggleOverlay()        { if (m_overlay) m_overlay->toggle(); }
bool MirrorWindow::isVisible()     const  { return m_running; }
uint32_t MirrorWindow::clientWidth()  const { return m_win32->width(); }
uint32_t MirrorWindow::clientHeight() const { return m_win32->height(); }

} // namespace aura
