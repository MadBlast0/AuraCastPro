#pragma once
// =============================================================================
// DX12Manager.h — DirectX 12 top-level manager.
//
// Owns the DXGI factory, selects the best GPU adapter, creates the D3D12
// device, and coordinates the initialisation of all DX12 subsystems:
//   DX12DeviceContext → DX12CommandQueue → DX12Fence
//
// Also manages the root signatures and pipeline state objects (PSOs) used
// by the rendering pipeline:
//   - NV12→RGB conversion PSO
//   - HDR tone-mapping PSO
//   - Lanczos scaling PSO
//
// Thread safety:
//   init() and shutdown() must be called from the main thread.
//   beginFrame() / endFrame() are called from the render thread.
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;

namespace aura {

class DX12DeviceContext;
class DX12CommandQueue;
class DX12Fence;

class DX12Manager {
public:
    DX12Manager();
    ~DX12Manager();

    // Call once from main() after HardwareProfiler::detect().
    // Throws std::runtime_error on any DirectX initialisation failure.
    void init();

    // Graceful shutdown — waits for GPU idle before releasing resources.
    void shutdown();

    bool isInitialised() const { return m_initialised; }

    // Raw D3D12 device (used by subsystems that need to create resources)
    ID3D12Device* device() const { return m_device.Get(); }

    // Subsystem accessors
    DX12DeviceContext* deviceContext() const { return m_deviceContext.get(); }
    DX12CommandQueue*  commandQueue()  const { return m_commandQueue.get(); }
    DX12Fence*         fence()         const { return m_fence.get(); }

    // Root signatures
    ID3D12RootSignature* renderRootSignature() const { return m_renderRS.Get(); }

    // PSO accessors
    ID3D12PipelineState* nv12ToRgbPSO()     const { return m_nv12ToRgbPSO.Get(); }
    ID3D12PipelineState* hdrTonemapPSO()    const { return m_hdrTonemapPSO.Get(); }
    ID3D12PipelineState* lanczosPSO()       const { return m_lanczosPSO.Get(); }
    ID3D12PipelineState* chromaUpsamplePSO() const { return m_chromaUpsamplePSO.Get(); }
    ID3D12PipelineState* temporalPacingPSO() const { return m_temporalPacingPSO.Get(); }

    // GPU adapter name (for UI display)
    const std::string& adapterName() const { return m_adapterName; }

    // Force GPU to finish all queued work (blocks CPU until idle)
    void waitForGPUIdle();

private:
    bool m_initialised{false};
    std::string m_adapterName;

    ComPtr<IDXGIFactory6>   m_factory;
    ComPtr<IDXGIAdapter1>   m_adapter;
    ComPtr<ID3D12Device>    m_device;

    // In debug builds, enable the D3D12 debug layer for validation
    ComPtr<ID3D12Debug>     m_debugLayer;

    // Sub-managers
    std::unique_ptr<DX12DeviceContext> m_deviceContext;
    std::unique_ptr<DX12CommandQueue>  m_commandQueue;
    std::unique_ptr<DX12Fence>         m_fence;

    // Root signatures
    ComPtr<ID3D12RootSignature> m_renderRS;

    // Pipeline state objects
    ComPtr<ID3D12PipelineState> m_nv12ToRgbPSO;
    ComPtr<ID3D12PipelineState> m_hdrTonemapPSO;
    ComPtr<ID3D12PipelineState> m_chromaUpsamplePSO;  // chroma upsampling (4:2:0→4:4:4)
    ComPtr<ID3D12PipelineState> m_temporalPacingPSO;  // temporal frame pacing CS
    ComPtr<ID3D12RootSignature> m_computeRS;           // root sig for compute shaders
    ComPtr<ID3D12PipelineState> m_lanczosPSO;

    void enableDebugLayer();
    void createFactory();
    void selectAdapter();
    void createDevice();
    void createRootSignatures();
    void loadShaderAndCreatePSOs();
    std::vector<uint8_t> loadShaderBlob(const std::string& filename);
};

} // namespace aura
