#pragma once
// =============================================================================
// MirrorWindow.h -- Mirror window combining Win32 HWND + DX12 render loop
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace aura {
class DX12Manager;
class MirrorWindowWin32;
class PerformanceOverlay;
class VCamBridge;

class MirrorWindow {
public:
    MirrorWindow(DX12Manager* dx12, VCamBridge* vcam = nullptr);
    ~MirrorWindow();

    void init();
    void start();    // Create window + DX12 swapchain, show window
    void stop();     // GPU flush, hide window
    void shutdown(); // Release all DX12 resources

    // Called each decoded frame: records command list, calls Present()
    void presentFrame(uint32_t width, uint32_t height);

    // Provide a newly-decoded NV12 D3D12 texture for the next frame
    void setCurrentTexture(ID3D12Resource* texture);

    void setTitle(const std::string& title);
    void setFullscreen(bool fs);
    void toggleOverlay();
    
    void show() { m_running = true; }
    void hide() { m_running = false; }
    bool isFullscreen() const { return false; }
    void toggleFullscreen() { setFullscreen(!isFullscreen()); }

    bool     isVisible()     const;
    uint32_t clientWidth()   const;
    uint32_t clientHeight()  const;

    void setOverlay(PerformanceOverlay* overlay) { m_overlay = overlay; }

private:
    DX12Manager*        m_dx12;
    VCamBridge*         m_vcam;
    PerformanceOverlay* m_overlay{nullptr};

    std::unique_ptr<MirrorWindowWin32> m_win32;

    bool     m_running{false};
    uint32_t m_width{1280};
    uint32_t m_height{720};

    ComPtr<ID3D12Resource> m_currentTexture;
    UINT m_currentTextureWidth{1920};   // updated on each frame upload
    UINT m_currentTextureHeight{1080};  // used for texel size CBV constant

    struct RenderState;
    std::unique_ptr<RenderState> m_render;

    void createSwapchainAndResources();
    void onResize(uint32_t w, uint32_t h);
};

} // namespace aura
