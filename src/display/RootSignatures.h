#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <directx/d3dx12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Pre-compiled root signatures for every render pass.
// Created once at startup, used by PipelineStateObjects.
class RootSignatures {
public:
    enum class RS {
        NV12ToRGB = 0,
        HDRTonemap,
        Lanczos,
        ChromaUpsample,
        FramePacing,
        Count
    };

    bool init(ID3D12Device* device);
    void release();

    ID3D12RootSignature* get(RS rs) const {
        return m_rs[static_cast<int>(rs)].Get();
    }

private:
    bool createNV12ToRGB(ID3D12Device* device);
    bool createHDRTonemap(ID3D12Device* device);
    bool createLanczos(ID3D12Device* device);
    bool createChromaUpsample(ID3D12Device* device);
    bool createFramePacing(ID3D12Device* device);

    bool finalise(ID3D12Device* device,
                  const D3D12_ROOT_SIGNATURE_DESC& desc,
                  RS slot);

    ComPtr<ID3D12RootSignature> m_rs[static_cast<int>(RS::Count)];
};
