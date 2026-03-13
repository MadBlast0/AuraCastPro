#pragma once
// =============================================================================
// MirrorWindowWin32.h -- Win32 window host for the DX12 mirror swapchain
// Task 111: Swapchain resize wired -- WM_SIZE triggers ResizeBuffers safely
// Task 113: Rotation wired -- landscape/portrait transitions handled
// Task 179: setAlwaysOnTop() wired to HWND_TOPMOST / HWND_NOTOPMOST
// =============================================================================
#include <cstdint>
#include <functional>
#include <atomic>
#include <string>

struct HWND__;
using HWND = HWND__*;
struct IDXGISwapChain4;
struct IDXGISwapChain4;
struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12DescriptorHeap;

namespace aura {

class PerformanceOverlay; // forward declare

class MirrorWindowWin32 {
public:
    using ResizeCallback   = std::function<void(uint32_t w, uint32_t h)>;
    using KeyCallback      = std::function<void(uint32_t vkCode)>;
    using CloseCallback    = std::function<void()>;
    using RotationCallback = std::function<void(uint32_t w, uint32_t h)>;
    using DpiChangedCallback = std::function<void(uint32_t newDpi)>; // Task 110

    MirrorWindowWin32();
    ~MirrorWindowWin32();

    void init();
    void create(uint32_t width, uint32_t height, const std::string& title);
    void show();
    void hide();
    void shutdown();

    void setFullscreen(bool fs);
    void toggleFullscreen();
    bool isFullscreen() const { return m_fullscreen.load(); }

    // Task 179: Always-on-top (called from HubWindow settings toggle)
    void setAlwaysOnTop(bool onTop);
    bool isAlwaysOnTop() const { return m_alwaysOnTop.load(); }

    // PerformanceOverlay -- shown on top of the video frame (Ctrl+Shift+O)
    // The overlay is rendered as a QML window child of the mirror HWND.
    void setOverlay(aura::PerformanceOverlay* overlay) { m_overlay = overlay; }

    HWND hwnd() const { return m_hwnd; }
    uint32_t width()  const { return m_width; }
    uint32_t height() const { return m_height; }

    // Create DXGI swapchain attached to this window
    IDXGISwapChain4* createSwapChain(ID3D12CommandQueue* queue,
                                     uint32_t bufferCount = 2);

    // Task 111: Bind swapchain + RTV heap for resize handling
    void bindSwapchain(IDXGISwapChain4*       swapchain,
                       ID3D12Device*          device,
                       ID3D12DescriptorHeap*  rtvHeap,
                       uint32_t               bufferCount);

    void setResizeCallback(ResizeCallback cb)   { m_onResize   = std::move(cb); }
    void setKeyCallback(KeyCallback cb)         { m_onKey      = std::move(cb); }
    void setCloseCallback(CloseCallback cb)     { m_onClose    = std::move(cb); }
    // Task 113: Called after swapchain resize to update Lanczos/shader viewport
    void setRotationCallback(RotationCallback cb){ m_onRotation = std::move(cb); }
    // Task 110: Called when window moves to a monitor with different DPI
    void setDpiChangedCallback(DpiChangedCallback cb) { m_onDpiChanged = std::move(cb); }
    uint32_t dpi() const { return m_dpi; }

private:
    HWND              m_hwnd{nullptr};
    uint32_t          m_width{1280};
    uint32_t          m_height{720};
    std::atomic<bool> m_fullscreen{false};
    std::atomic<bool> m_alwaysOnTop{false};  // Task 179
    PerformanceOverlay* m_overlay{nullptr};  // Task 172: stats overlay (not owned)

    // Task 111: Swapchain references for resize
    IDXGISwapChain4*      m_swapchain{nullptr};
    ID3D12Device*         m_device{nullptr};
    ID3D12DescriptorHeap* m_rtvHeap{nullptr};
    uint32_t              m_bufferCount{2};

    // Pending resize dimensions (set in WM_SIZE, applied before next Present)
    std::atomic<bool>     m_resizePending{false};
    uint32_t              m_pendingWidth{0};
    uint32_t              m_pendingHeight{0};

    ResizeCallback    m_onResize;
    KeyCallback       m_onKey;
    CloseCallback     m_onClose;
    RotationCallback  m_onRotation;   // Task 113
    DpiChangedCallback m_onDpiChanged; // Task 110
    uint32_t          m_dpi{96};       // Task 110: current DPI (default 96 = 100%)

    bool              m_registered{false};

    void applyPendingResize();         // Task 111: called from render loop

    static long __stdcall WndProc(HWND__* hwnd, unsigned msg,
                                   unsigned long long wParam, long long lParam);
};

} // namespace aura
