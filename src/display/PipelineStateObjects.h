#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include "RootSignatures.h"
#include <string>

using Microsoft::WRL::ComPtr;

// Pre-compiled pipeline state objects. Created once at startup.
// Each PSO = root signature + vertex shader + pixel shader + render state.
class PipelineStateObjects {
public:
    enum class PSO {
        NV12ToRGB = 0,
        HDRTonemap,
        Lanczos,
        ChromaUpsample,
        FramePacing,
        Count
    };

    // shaderDir: folder containing compiled .cso files
    bool init(ID3D12Device* device,
              RootSignatures* rootSigs,
              const std::wstring& shaderDir);
    void release();

    ID3D12PipelineState* get(PSO pso) const {
        return m_pso[static_cast<int>(pso)].Get();
    }

private:
    bool loadShader(const std::wstring& path,
                    std::vector<uint8_t>& bytecode);

    bool createPSO(ID3D12Device* device,
                   ID3D12RootSignature* rootSig,
                   const std::vector<uint8_t>& vsBytecode,
                   const std::vector<uint8_t>& psBytecode,
                   DXGI_FORMAT rtvFormat,
                   PSO slot);

    ComPtr<ID3D12PipelineState> m_pso[static_cast<int>(PSO::Count)];
};
