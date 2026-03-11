#include "../pch.h"  // PCH
#include "PipelineStateObjects.h"
#include "../utils/Logger.h"
#include <fstream>
#include <filesystem>

bool PipelineStateObjects::loadShader(const std::wstring& path,
                                       std::vector<uint8_t>& bytecode) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        LOG_ERROR("PSO: Cannot open shader: {}", std::filesystem::path(path).string());
        return false;
    }
    size_t size = f.tellg();
    f.seekg(0);
    bytecode.resize(size);
    f.read(reinterpret_cast<char*>(bytecode.data()), size);
    return true;
}

bool PipelineStateObjects::createPSO(ID3D12Device* device,
                                      ID3D12RootSignature* rootSig,
                                      const std::vector<uint8_t>& vs,
                                      const std::vector<uint8_t>& ps,
                                      DXGI_FORMAT rtvFormat,
                                      PSO slot) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature     = rootSig;
    desc.VS                 = { vs.data(), vs.size() };
    desc.PS                 = { ps.data(), ps.size() };
    desc.RasterizerState    = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.BlendState         = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable  = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.SampleMask         = UINT_MAX;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets   = 1;
    desc.RTVFormats[0]      = rtvFormat;
    desc.SampleDesc.Count   = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&m_pso[(int)slot]));
    if (FAILED(hr)) {
        LOG_ERROR("PSO: CreateGraphicsPipelineState failed for slot {}: 0x{:X}",
                  (int)slot, (uint32_t)hr);
        return false;
    }
    return true;
}

bool PipelineStateObjects::init(ID3D12Device* device,
                                 RootSignatures* rootSigs,
                                 const std::wstring& shaderDir) {
    // Full-screen triangle vertex shader (shared by all passes)
    std::vector<uint8_t> vsBytes, psBytes;
    auto vs = shaderDir + L"\\fullscreen_vs.cso";

    if (!loadShader(vs, vsBytes)) {
        LOG_WARN("PSO: fullscreen_vs.cso not found — using empty VS (will fail at draw)");
        vsBytes = {}; // Will compile-fail gracefully
    }

    struct PassDesc {
        PSO         slot;
        const wchar_t* psCso;
        RootSignatures::RS rs;
        DXGI_FORMAT rtvFmt;
    } passes[] = {
        { PSO::NV12ToRGB,      L"nv12_to_rgb.cso",          RootSignatures::RS::NV12ToRGB,     DXGI_FORMAT_R8G8B8A8_UNORM         },
        { PSO::HDRTonemap,     L"hdr10_tonemap.cso",         RootSignatures::RS::HDRTonemap,    DXGI_FORMAT_R16G16B16A16_FLOAT     },
        { PSO::Lanczos,        L"lanczos8.cso",              RootSignatures::RS::Lanczos,       DXGI_FORMAT_R8G8B8A8_UNORM         },
        { PSO::ChromaUpsample, L"chroma_upsample.cso",       RootSignatures::RS::ChromaUpsample,DXGI_FORMAT_R8G8B8A8_UNORM         },
        { PSO::FramePacing,    L"temporal_frame_pacing.cso", RootSignatures::RS::FramePacing,   DXGI_FORMAT_R8G8B8A8_UNORM         },
    };

    for (auto& p : passes) {
        if (!loadShader(shaderDir + L"\\" + p.psCso, psBytes)) {
            LOG_WARN("PSO: Shader {} not found — PSO will be null", (int)p.slot);
            continue;
        }
        if (!createPSO(device, rootSigs->get(p.rs), vsBytes, psBytes, p.rtvFmt, p.slot)) {
            return false;
        }
    }

    LOG_INFO("PipelineStateObjects: All PSOs created");
    return true;
}

void PipelineStateObjects::release() {
    for (auto& pso : m_pso) pso.Reset();
}
