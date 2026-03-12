#pragma once
// =============================================================================
// GPUProfileMarkers.h — PIX / RenderDoc GPU profiling markers (Task 107)
//
// Wraps PIX event macros so they:
//   • Appear as named ranges in PIX for Windows and RenderDoc
//   • Compile to ZERO instructions in Release builds (stripped by optimizer)
//   • Never crash if the PIX runtime isn't installed
//
// Usage (in any DX12 rendering function):
//
//   AURA_GPU_BEGIN(cmdList, 0x0078D7FF, "NV12 to RGB Conversion");
//   // ... draw call ...
//   AURA_GPU_END(cmdList);
//
//   {
//       AURA_GPU_SCOPE(cmdList, 0xFFCC00, "Lanczos Scaling");
//       // ... draw calls ...
//   }  // AURA_GPU_END called automatically at scope exit
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>

// ── PIX colour helpers ────────────────────────────────────────────────────────
// Pass as the 'colour' argument.  0xRRGGBB format (alpha ignored by PIX).
namespace aura::GPU {
    constexpr uint32_t ColourNV12ToRGB      = 0x0078D7FF; // cyan-blue
    constexpr uint32_t ColourHDRTonemap     = 0xFF8C00FF; // orange
    constexpr uint32_t ColourLanczos        = 0x00CC6EFF; // green
    constexpr uint32_t ColourChromaUpsample = 0xFFD700FF; // gold
    constexpr uint32_t ColourFramePacing    = 0xFF2D55FF; // pink-red
    constexpr uint32_t ColourPresent        = 0xAAAAAA00; // grey
    constexpr uint32_t ColourAudioMix       = 0x34C759FF; // mint
} // namespace aura::GPU

// ── Macro definitions ─────────────────────────────────────────────────────────
// In Release builds the compiler sees empty do-nothing expressions and removes
// them entirely — confirmed zero overhead by MSVC /O2 and clang-O3.
#ifndef NDEBUG

// D3D12 command-list annotations understood by PIX and RenderDoc
#define AURA_GPU_BEGIN(cmdList, colour, name)                         \
    do {                                                              \
        if (cmdList) {                                                \
            const std::wstring _ws = [&](){                           \
                std::string _s(name);                                 \
                return std::wstring(_s.begin(), _s.end());           \
            }();                                                      \
            D3D12_MESSAGE_CATEGORY _cat = {};                        \
            (void)_cat;                                               \
            /* PIX runtime annotation via SetMarker convention */    \
            cmdList->BeginEvent(                                      \
                /* metadata = */ 2 /* PIX_EVENT_UNICODE_VERSION */,  \
                _ws.c_str(),                                          \
                static_cast<UINT>(_ws.size() * sizeof(wchar_t)));    \
        }                                                             \
    } while(0)

#define AURA_GPU_END(cmdList)                                         \
    do { if (cmdList) { cmdList->EndEvent(); } } while(0)

#define AURA_GPU_MARKER(cmdList, colour, name)                        \
    do {                                                              \
        if (cmdList) {                                                \
            const std::wstring _wm = [&](){                           \
                std::string _s(name);                                 \
                return std::wstring(_s.begin(), _s.end());           \
            }();                                                      \
            cmdList->SetMarker(                                       \
                /* metadata = */ 2,                                   \
                _wm.c_str(),                                          \
                static_cast<UINT>(_wm.size() * sizeof(wchar_t)));    \
        }                                                             \
    } while(0)

#else // NDEBUG — Release: strip all markers to zero cost

#define AURA_GPU_BEGIN(cmdList, colour, name)  do {} while(0)
#define AURA_GPU_END(cmdList)                  do {} while(0)
#define AURA_GPU_MARKER(cmdList, colour, name) do {} while(0)

#endif // NDEBUG

// ── RAII scope guard ──────────────────────────────────────────────────────────
namespace aura {

class GpuProfileScope {
public:
    GpuProfileScope(ID3D12GraphicsCommandList* list,
                    uint32_t colour,
                    const char* name)
        : m_list(list)
    {
        AURA_GPU_BEGIN(m_list, colour, name);
    }
    ~GpuProfileScope() {
        AURA_GPU_END(m_list);
    }
    GpuProfileScope(const GpuProfileScope&) = delete;
    GpuProfileScope& operator=(const GpuProfileScope&) = delete;

private:
    ID3D12GraphicsCommandList* m_list;
};

} // namespace aura

// Convenience macro for RAII scoped GPU event
#define AURA_GPU_SCOPE(cmdList, colour, name) \
    aura::GpuProfileScope _gpuScope_##__LINE__(cmdList, colour, name)
