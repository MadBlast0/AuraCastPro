// =============================================================================
// DX12Manager.cpp — DirectX 12 device creation and PSO management
// =============================================================================

#include "../pch.h"  // PCH
#include "DX12Manager.h"
#include "DX12DeviceContext.h"
#include "DX12CommandQueue.h"
#include "DX12Fence.h"
#include "../utils/Logger.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <format>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace aura {

// Macro that throws a descriptive error on HRESULT failure
#define DX_CHECK(hr, msg) \
    do { if (FAILED(hr)) { \
        throw std::runtime_error(std::format("{}: HRESULT={:08X}", (msg), (uint32_t)(hr))); \
    } } while(0)

// -----------------------------------------------------------------------------
DX12Manager::DX12Manager() = default;
DX12Manager::~DX12Manager() { shutdown(); }

// -----------------------------------------------------------------------------
void DX12Manager::init() {
    AURA_LOG_INFO("DX12Manager", "Initialising DirectX 12...");

#ifdef _DEBUG
    enableDebugLayer();
#endif

    createFactory();
    selectAdapter();
    createDevice();

    // Initialise sub-managers in dependency order
    m_deviceContext = std::make_unique<DX12DeviceContext>(m_device.Get());
    m_deviceContext->init();

    m_commandQueue = std::make_unique<DX12CommandQueue>(m_device.Get());
    m_commandQueue->init();

    m_fence = std::make_unique<DX12Fence>(m_device.Get(), m_commandQueue->queue());
    m_fence->init();

    createRootSignatures();
    loadShaderAndCreatePSOs();

    m_initialised = true;
    AURA_LOG_INFO("DX12Manager", "DirectX 12 ready. Adapter: {}", m_adapterName);
}

// -----------------------------------------------------------------------------
void DX12Manager::shutdown() {
    if (!m_initialised) return;

    AURA_LOG_INFO("DX12Manager", "Shutting down DirectX 12...");

    waitForGPUIdle();

    m_computeRS.Reset();
    m_temporalPacingPSO.Reset();
    m_chromaUpsamplePSO.Reset();
    m_lanczosPSO.Reset();
    m_hdrTonemapPSO.Reset();
    m_nv12ToRgbPSO.Reset();
    m_renderRS.Reset();

    m_fence.reset();
    m_commandQueue.reset();
    m_deviceContext.reset();

    m_device.Reset();
    m_adapter.Reset();
    m_factory.Reset();

#ifdef _DEBUG
    // Report live D3D12 objects (helps detect leaks)
    ComPtr<ID3D12DebugDevice> debugDevice;
    if (m_device && SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&debugDevice)))) {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }
    m_debugLayer.Reset();
#endif

    m_initialised = false;
    AURA_LOG_INFO("DX12Manager", "DirectX 12 shutdown complete.");
}

// -----------------------------------------------------------------------------
void DX12Manager::enableDebugLayer() {
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugLayer)))) {
        m_debugLayer->EnableDebugLayer();
        AURA_LOG_DEBUG("DX12Manager", "D3D12 debug layer enabled.");
    }
}

// -----------------------------------------------------------------------------
void DX12Manager::createFactory() {
    const UINT flags = 0;
#ifdef _DEBUG
    // Enable DXGI debug layer messages in debug builds
    // flags = DXGI_CREATE_FACTORY_DEBUG; // Uncomment if desired
#endif
    DX_CHECK(CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)),
             "DX12Manager: CreateDXGIFactory2");
}

// -----------------------------------------------------------------------------
void DX12Manager::selectAdapter() {
    // Enumerate adapters, preferring high-performance (discrete GPU)
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_factory->EnumAdapterByGpuPreference(
             i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++i) {

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) continue;  // skip unenumerable adapter

        // Skip software / WARP adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        // Test D3D12 support without creating a real device
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
            m_adapter = adapter;

            char nameBuf[256]{};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                                nameBuf, sizeof(nameBuf), nullptr, nullptr);
            m_adapterName = nameBuf;

            AURA_LOG_INFO("DX12Manager", "Selected adapter [{}]: {} ({} MB VRAM)",
                          i, m_adapterName, desc.DedicatedVideoMemory / (1024 * 1024));
            return;
        }
    }
    throw std::runtime_error("DX12Manager: No D3D12-capable GPU found. "
                              "A DirectX 12 compatible GPU is required.");
}

// -----------------------------------------------------------------------------
void DX12Manager::createDevice() {
    DX_CHECK(
        D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)),
        "DX12Manager: D3D12CreateDevice");

    // In debug builds, configure the info queue to break on errors
#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    }
#endif

    AURA_LOG_DEBUG("DX12Manager", "D3D12 device created (feature level 11_0+).");
}

// -----------------------------------------------------------------------------
void DX12Manager::createRootSignatures() {
    // Render root signature:
    //   [0] CBV: per-draw constants (transform matrix, color space params)
    //   [1] SRV table: input texture (NV12 or RGB)
    //   [2] Static sampler: linear clamp

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors     = 2; // Y plane + UV plane for NV12
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace      = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[2]{};
    // CBV at register b0 — per-draw constants
    params[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].Descriptor.RegisterSpace  = 0;
    params[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    // Descriptor table for SRV(s)
    params[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    params[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static linear clamp sampler at register s0
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister   = 0;
    sampler.RegisterSpace    = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters     = 2;
    rsDesc.pParameters       = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers   = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialised, error;
    DX_CHECK(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &serialised, &error),
             "DX12Manager: D3D12SerializeRootSignature");

    DX_CHECK(m_device->CreateRootSignature(0,
                serialised->GetBufferPointer(), serialised->GetBufferSize(),
                IID_PPV_ARGS(&m_renderRS)),
             "DX12Manager: CreateRootSignature");

    AURA_LOG_DEBUG("DX12Manager", "Root signature created.");

    // Compute root signature for temporal_frame_pacing CS
    {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors     = 2;
        srvRange.BaseShaderRegister = 0;

        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors     = 1;
        uavRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER cparams[3]{};
        cparams[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        cparams[0].DescriptorTable.NumDescriptorRanges = 1;
        cparams[0].DescriptorTable.pDescriptorRanges   = &srvRange;
        cparams[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
        cparams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        cparams[1].DescriptorTable.NumDescriptorRanges = 1;
        cparams[1].DescriptorTable.pDescriptorRanges   = &uavRange;
        cparams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
        cparams[2].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        cparams[2].Constants.Num32BitValues = 4;
        cparams[2].Constants.ShaderRegister = 0;
        cparams[2].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC crsDesc{};
        crsDesc.NumParameters = 3;
        crsDesc.pParameters   = cparams;
        crsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> crsBlob, crsErr;
        if (SUCCEEDED(D3D12SerializeRootSignature(&crsDesc,
                D3D_ROOT_SIGNATURE_VERSION_1_0, &crsBlob, &crsErr))) {
            m_device->CreateRootSignature(0,
                crsBlob->GetBufferPointer(), crsBlob->GetBufferSize(),
                IID_PPV_ARGS(&m_computeRS));
            AURA_LOG_DEBUG("DX12Manager", "Compute root signature created.");
        } else {
            AURA_LOG_WARN("DX12Manager",
                "Compute root signature serialization failed — temporal pacing PSO skipped.");
        }
    }
}

// -----------------------------------------------------------------------------
std::vector<uint8_t> DX12Manager::loadShaderBlob(const std::string& filename) {
    // Shaders are compiled to .cso files by the CMake build step.
    // They live next to the executable.
    std::filesystem::path exeDir = std::filesystem::current_path();
    const std::filesystem::path shaderPath = exeDir / "Shaders" / filename;

    std::ifstream f(shaderPath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        throw std::runtime_error(std::format(
            "DX12Manager: Cannot open shader: {}", shaderPath.string()));
    }

    const std::size_t size = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> blob(size);
    f.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(size));
    return blob;
}

// -----------------------------------------------------------------------------
void DX12Manager::loadShaderAndCreatePSOs() {
    // Full-screen triangle input layout (no vertex buffer — position computed in VS)
    // For our rendering pipeline we use a vertex-less approach:
    // The vertex shader generates a full-screen triangle from SV_VertexID.

    // Load the shared vertex shader once — all PSOs use the same fullscreen triangle VS
    auto vs = loadShaderBlob("fullscreen_vs.cso");

    // NV12 → RGB conversion PSO
    {
        auto ps = loadShaderBlob("nv12_to_rgb.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_renderRS.Get();
        psoDesc.VS             = { vs.data(), vs.size() };  // fullscreen triangle VS (required)
        psoDesc.PS             = { ps.data(), ps.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState.DepthEnable   = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask                      = UINT_MAX;
        psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                = 1;
        psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count                = 1;

        DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_nv12ToRgbPSO)),
                 "DX12Manager: CreateGraphicsPipelineState (nv12_to_rgb)");
        AURA_LOG_DEBUG("DX12Manager", "PSO created: nv12_to_rgb");
    }

    // HDR tone-mapping PSO — similar structure, different pixel shader
    {
        auto ps = loadShaderBlob("hdr10_tonemap.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_renderRS.Get();
        psoDesc.VS             = { vs.data(), vs.size() };  // fullscreen triangle VS (required)
        psoDesc.PS             = { ps.data(), ps.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState.DepthEnable   = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask                      = UINT_MAX;
        psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                = 1;
        psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R10G10B10A2_UNORM;
        psoDesc.SampleDesc.Count                = 1;

        DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_hdrTonemapPSO)),
                 "DX12Manager: CreateGraphicsPipelineState (hdr10_tonemap)");
        AURA_LOG_DEBUG("DX12Manager", "PSO created: hdr10_tonemap");
    }

    // Lanczos scaling PSO
    {
        auto ps = loadShaderBlob("lanczos8.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_renderRS.Get();
        psoDesc.VS             = { vs.data(), vs.size() };  // fullscreen triangle VS (required)
        psoDesc.PS             = { ps.data(), ps.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState.DepthEnable   = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask                      = UINT_MAX;
        psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                = 1;
        psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count                = 1;

        DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_lanczosPSO)),
                 "DX12Manager: CreateGraphicsPipelineState (lanczos8)");
        AURA_LOG_DEBUG("DX12Manager", "PSO created: lanczos8");
    }

    // Chroma upsampling PSO (4:2:0 → 4:4:4 pixel shader)
    {
        auto ps = loadShaderBlob("chroma_upsample.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_renderRS.Get();
        psoDesc.VS             = { vs.data(), vs.size() };
        psoDesc.PS             = { ps.data(), ps.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState.DepthEnable   = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask                      = UINT_MAX;
        psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets                = 1;
        psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count                = 1;
        DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_chromaUpsamplePSO)),
                 "DX12Manager: CreateGraphicsPipelineState (chroma_upsample)");
        AURA_LOG_DEBUG("DX12Manager", "PSO created: chroma_upsample");
    }

    // Temporal frame pacing compute shader PSO
    if (m_computeRS) {
    {
        auto cs = loadShaderBlob("temporal_frame_pacing.cso");
        D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc{};
        cpsoDesc.pRootSignature = m_computeRS.Get();
        cpsoDesc.CS             = { cs.data(), cs.size() };
        DX_CHECK(m_device->CreateComputePipelineState(&cpsoDesc, IID_PPV_ARGS(&m_temporalPacingPSO)),
                 "DX12Manager: CreateComputePipelineState (temporal_frame_pacing)");
        AURA_LOG_DEBUG("DX12Manager", "PSO created: temporal_frame_pacing (compute)");
    }
    } // if (m_computeRS)
}

// -----------------------------------------------------------------------------
void DX12Manager::waitForGPUIdle() {
    if (m_fence && m_commandQueue) {
        m_fence->signalAndWait();
    }
}

} // namespace aura
