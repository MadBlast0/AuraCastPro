// =============================================================================
// MirrorWindowWin32.cpp — Win32 mirror window with DXGI swapchain
// Task 111: bindSwapchain() + applyPendingResize() wired
// Task 113: Rotation callback wired through resize path
// Task 179: setAlwaysOnTop() wired to HWND_TOPMOST
// =============================================================================

#include "../pch.h"  // PCH
#include "MirrorWindowWin32.h"
#include "DisplayHelpers.h"      // Task 111: SwapchainResize::execute()
#include "GPUProfileMarkers.h"   // Task 107: AURA_GPU_BEGIN / AURA_GPU_END macros
#include "HDRDetection.h"        // Task 110: HDR color space detection
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <stdexcept>
#include <format>

#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace aura {

static constexpr wchar_t kClassName[] = L"AuraCastProMirrorWindow";

// -----------------------------------------------------------------------------
MirrorWindowWin32::MirrorWindowWin32() {}

MirrorWindowWin32::~MirrorWindowWin32() { shutdown(); }

// -----------------------------------------------------------------------------
void MirrorWindowWin32::init() {
    if (m_registered) return;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOWFRAME);
    wc.lpszClassName = kClassName;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::runtime_error(
                std::format("MirrorWindowWin32: RegisterClassExW failed: {}", err));
        }
    }
    m_registered = true;
    AURA_LOG_DEBUG("MirrorWindowWin32", "Window class registered.");
}

// -----------------------------------------------------------------------------
void MirrorWindowWin32::create(uint32_t width, uint32_t height,
                                const std::string& title) {
    m_width  = width;
    m_height = height;

    const DWORD style   = WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = WS_EX_APPWINDOW;

    RECT rc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    const std::wstring wtitle(title.begin(), title.end());

    m_hwnd = CreateWindowExW(
        exStyle,
        kClassName,
        wtitle.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        this
    );

    if (!m_hwnd) {
        throw std::runtime_error(
            std::format("MirrorWindowWin32: CreateWindowExW failed: {}",
                        GetLastError()));
    }

    AURA_LOG_INFO("MirrorWindowWin32", "Window created: {}×{}", width, height);
}

// -----------------------------------------------------------------------------
void MirrorWindowWin32::show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_hwnd);
    }
}

void MirrorWindowWin32::hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

// -----------------------------------------------------------------------------
// Task 111: Store swapchain + RTV heap so applyPendingResize can call
//           SwapchainResize::execute() without a raw D3D device pointer
//           being passed into WM_SIZE (which runs on the message thread).
void MirrorWindowWin32::bindSwapchain(IDXGISwapChain4*      swapchain,
                                       ID3D12Device*         device,
                                       ID3D12DescriptorHeap* rtvHeap,
                                       uint32_t              bufferCount)
{
    m_swapchain   = swapchain;
    m_device      = device;
    m_rtvHeap     = rtvHeap;
    m_bufferCount = bufferCount;
    AURA_LOG_DEBUG("MirrorWindowWin32",
        "Swapchain bound for resize tracking ({} buffers).", bufferCount);
}

// -----------------------------------------------------------------------------
// Task 111: Called from the render loop (same thread as DX12 submission)
//           BEFORE recording the next frame's command list.
void MirrorWindowWin32::applyPendingResize() {
    if (!m_resizePending.exchange(false)) return;

    const uint32_t newW = m_pendingWidth;
    const uint32_t newH = m_pendingHeight;

    if (!m_swapchain || !m_device || newW == 0 || newH == 0) return;

    AURA_LOG_INFO("MirrorWindowWin32",
        "Applying pending swapchain resize: {}×{}", newW, newH);

    // SwapchainResize::execute() waits for GPU idle, calls ResizeBuffers,
    // then re-creates RTVs from m_rtvHeap.
    const bool ok = aura::SwapchainResize::execute(
        reinterpret_cast<IDXGISwapChain4*>(m_swapchain),
        m_device,
        m_rtvHeap,
        newW, newH,
        m_bufferCount);

    if (!ok) {
        AURA_LOG_ERROR("MirrorWindowWin32",
            "SwapchainResize::execute failed for {}×{}", newW, newH);
        return;
    }

    m_width  = newW;
    m_height = newH;

    // Task 113: Notify RotationHandler / shader viewport of new dimensions
    if (m_onRotation) {
        m_onRotation(newW, newH);
    }

    // Also fire the generic resize callback so NetworkStatsModel / overlay
    // can update their aspect ratio
    if (m_onResize) {
        m_onResize(newW, newH);
    }
}

// -----------------------------------------------------------------------------
IDXGISwapChain4* MirrorWindowWin32::createSwapChain(
    ID3D12CommandQueue* queue, uint32_t bufferCount)
{
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindowWin32",
            "CreateDXGIFactory2 failed: {:08X}", (uint32_t)hr);
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width              = m_width;
    desc.Height             = m_height;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount        = bufferCount;
    desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        queue, m_hwnd, &desc, nullptr, nullptr, &swapChain1);
    if (FAILED(hr)) {
        AURA_LOG_ERROR("MirrorWindowWin32",
            "CreateSwapChainForHwnd failed: {:08X}", (uint32_t)hr);
        return nullptr;
    }

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    IDXGISwapChain4* swapChain4 = nullptr;
    swapChain1->QueryInterface(IID_PPV_ARGS(&swapChain4));

    // ── HDR color space detection (Task 110) ──────────────────────────────────
    // Query the monitor for HDR capability and set the appropriate color space
    // so the GPU tone-mapping shader outputs correctly for HDR/SDR monitors.
    if (swapChain4) {
        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(factory.As(&factory6))) {
            HDRCapabilities hdrCaps = HDRDetection::query(m_hwnd, factory6.Get());
            if (hdrCaps.supportsHDR10) {
                // HDR10: use BT.2020 PQ color space + R16G16B16A16_FLOAT format
                HRESULT hrCS = swapChain4->SetColorSpace1(
                    DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
                if (SUCCEEDED(hrCS)) {
                    AURA_LOG_INFO("MirrorWindowWin32",
                        "HDR10 color space set on swapchain "
                        "(maxLum={:.0f} nits).", hdrCaps.maxLuminanceNits);
                } else {
                    AURA_LOG_WARN("MirrorWindowWin32",
                        "SetColorSpace1(HDR10) failed: {:08X} — using SDR.", (uint32_t)hrCS);
                }
            } else {
                // SDR: use standard sRGB — explicit set ensures correctness
                swapChain4->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
                AURA_LOG_DEBUG("MirrorWindowWin32",
                    "SDR monitor detected — sRGB color space applied.");
            }
        }
    }

    AURA_LOG_INFO("MirrorWindowWin32",
        "Swapchain created: {}×{} / {} buffers / FLIP_DISCARD",
        m_width, m_height, bufferCount);

    return swapChain4;
}

// -----------------------------------------------------------------------------
void MirrorWindowWin32::setFullscreen(bool fs) {
    if (m_fullscreen.load() == fs) return;
    m_fullscreen.store(fs);

    if (!m_hwnd) return;

    if (fs) {
        const DWORD fsStyle = WS_POPUP | WS_VISIBLE;
        SetWindowLongW(m_hwnd, GWL_STYLE, fsStyle);

        HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(hmon, &mi);
        SetWindowPos(m_hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right  - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
        AURA_LOG_DEBUG("MirrorWindowWin32", "Fullscreen ON");
    } else {
        SetWindowLongW(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(m_hwnd, nullptr,
                     100, 100, m_width, m_height,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
        AURA_LOG_DEBUG("MirrorWindowWin32", "Fullscreen OFF");
    }
}

void MirrorWindowWin32::toggleFullscreen() {
    setFullscreen(!m_fullscreen.load());
}

// -----------------------------------------------------------------------------
// Task 179: Always-on-top — wires the SettingsModel alwaysOnTop toggle to
//           the Win32 HWND z-order (HWND_TOPMOST / HWND_NOTOPMOST).
void MirrorWindowWin32::setAlwaysOnTop(bool onTop) {
    if (m_alwaysOnTop.load() == onTop) return;
    m_alwaysOnTop.store(onTop);

    if (!m_hwnd) return;

    const HWND insertAfter = onTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(m_hwnd, insertAfter,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    AURA_LOG_INFO("MirrorWindowWin32",
        "Always-on-top: {}", onTop ? "ON" : "OFF");
}

// -----------------------------------------------------------------------------
void MirrorWindowWin32::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// -----------------------------------------------------------------------------
long __stdcall MirrorWindowWin32::WndProc(
    HWND__* hwnd, unsigned msg, unsigned long long wParam, long long lParam)
{
    MirrorWindowWin32* self = nullptr;

    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MirrorWindowWin32*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MirrorWindowWin32*>(
                   GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
            case WM_SIZE: {
                // Task 111: Don't call ResizeBuffers here (wrong thread).
                // Record pending dimensions; render loop calls applyPendingResize()
                // before the next frame, ensuring GPU work is idle first.
                const uint32_t newW = LOWORD(lParam);
                const uint32_t newH = HIWORD(lParam);
                if (newW > 0 && newH > 0 &&
                    (newW != self->m_width || newH != self->m_height))
                {
                    self->m_pendingWidth  = newW;
                    self->m_pendingHeight = newH;
                    self->m_resizePending.store(true);
                }
                return 0;
            }

            case WM_KEYDOWN:
                if (wParam == 'F') self->toggleFullscreen();
                if (wParam == VK_ESCAPE && self->m_fullscreen.load())
                    self->setFullscreen(false);
                if (self->m_onKey) self->m_onKey(static_cast<uint32_t>(wParam));
                return 0;

            case WM_LBUTTONDBLCLK:
                self->toggleFullscreen();
                return 0;

            // Task 110: DPI change — resize to suggested rect and update scale factor.
            // Fires when window is dragged between monitors of different DPI, or when
            // the user changes their display scale in Windows settings.
            case WM_DPICHANGED: {
                // HIWORD(wParam) = new DPI (e.g. 96, 120, 144, 192)
                UINT newDpi = HIWORD(wParam);
                // lParam = RECT* suggested by Windows for the new window position/size
                const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
                if (suggested) {
                    SetWindowPos(hwnd, nullptr,
                        suggested->left, suggested->top,
                        suggested->right  - suggested->left,
                        suggested->bottom - suggested->top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                }
                // Store new DPI for coordinate scaling
                self->m_dpi = newDpi;
                // Re-check HDR capability (monitor may have changed)
                if (self->m_onDpiChanged) self->m_onDpiChanged(newDpi);
                return 0;
            }

            case WM_DESTROY:
                if (self->m_onClose) self->m_onClose();
                PostQuitMessage(0);
                return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace aura
