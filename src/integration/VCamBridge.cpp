// =============================================================================
// VCamBridge.cpp — Shared memory bridge to MirrorCam.dll virtual camera
//
// Shared memory layout: "Global\\AuraCastPro_VCam_v1"
//
// struct VCamSharedFrame {
//   uint32_t magic;         // 0xAC570001 — sanity check
//   uint32_t width, height; // current frame dimensions
//   uint32_t frameIndex;    // incremented each new frame
//   uint32_t format;        // 0=BGRA32, 1=NV12
//   uint64_t timestampUs;   // presentation timestamp
//   uint8_t  pixels[7680 * 4320 * 4]; // max 4K BGRA (128 MB)
// };
//
// MirrorCam.dll polls frameIndex and copies new frames to DirectShow.
// =============================================================================
#include "../pch.h"  // PCH
#include "VCamBridge.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <algorithm>

namespace aura {

constexpr uint32_t VCAM_MAGIC = 0xAC570001;
constexpr uint32_t MAX_WIDTH  = 3840;
constexpr uint32_t MAX_HEIGHT = 2160;
constexpr size_t   PIXEL_BYTES = MAX_WIDTH * MAX_HEIGHT * 4; // BGRA

#pragma pack(push, 1)
struct VCamSharedFrame {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t frameIndex;
    uint32_t format;       // 0=BGRA32
    uint64_t timestampUs;
    uint8_t  pixels[PIXEL_BYTES];
};
#pragma pack(pop)

struct VCamBridge::SharedState {
    HANDLE          hMapping{nullptr};
    VCamSharedFrame* frame{nullptr};
    HANDLE          hFrameReady{nullptr}; // auto-reset event
};

VCamBridge::VCamBridge() : m_shared(std::make_unique<SharedState>()) {}
VCamBridge::~VCamBridge() { shutdown(); }

void VCamBridge::init() {
    if (!createSharedMemory(MAX_WIDTH, MAX_HEIGHT)) {
        AURA_LOG_ERROR("VCamBridge", "Failed to create shared memory region.");
        return;
    }
    AURA_LOG_INFO("VCamBridge",
        "Shared memory created: 'Global\\AuraCastPro_VCam_v1' ({} MB). "
        "MirrorCam.dll will read frames from this region.",
        sizeof(VCamSharedFrame) / (1024 * 1024));
}

bool VCamBridge::createSharedMemory(uint32_t /*w*/, uint32_t /*h*/) {
    m_shared->hMapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        static_cast<DWORD>(sizeof(VCamSharedFrame) >> 32),
        static_cast<DWORD>(sizeof(VCamSharedFrame) & 0xFFFFFFFF),
        L"Global\\AuraCastPro_VCam_v1");

    if (!m_shared->hMapping) {
        AURA_LOG_WARN("VCamBridge",
            "CreateFileMapping failed: {}. "
            "Try running as Administrator for Global\\ namespace.", GetLastError());
        // Fall back to local namespace (won't be accessible to MirrorCam.dll
        // when running in a different session, but works for testing)
        m_shared->hMapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            static_cast<DWORD>(sizeof(VCamSharedFrame) >> 32),
            static_cast<DWORD>(sizeof(VCamSharedFrame) & 0xFFFFFFFF),
            L"AuraCastPro_VCam_v1");
    }

    if (!m_shared->hMapping) return false;

    m_shared->frame = static_cast<VCamSharedFrame*>(
        MapViewOfFile(m_shared->hMapping, FILE_MAP_ALL_ACCESS,
                      0, 0, sizeof(VCamSharedFrame)));

    if (!m_shared->frame) return false;

    m_shared->frame->magic      = VCAM_MAGIC;
    m_shared->frame->frameIndex = 0;

    m_shared->hFrameReady = CreateEventW(
        nullptr, FALSE, FALSE, L"AuraCastPro_VCam_FrameReady");

    return true;
}

void VCamBridge::start() {
    m_running.store(true);
    AURA_LOG_INFO("VCamBridge", "Virtual camera output active.");
}

void VCamBridge::stop() {
    m_running.store(false);
}

void VCamBridge::pushFrame(const uint8_t* bgraPixels, uint32_t width,
                            uint32_t height, uint64_t timestampUs) {
    if (!m_running.load() || !m_shared->frame) return;

    const uint32_t w = std::min(width,  MAX_WIDTH);
    const uint32_t h = std::min(height, MAX_HEIGHT);
    const size_t   copyBytes = static_cast<size_t>(w) * h * 4;

    m_shared->frame->width       = w;
    m_shared->frame->height      = h;
    m_shared->frame->format      = 0; // BGRA32
    m_shared->frame->timestampUs = timestampUs;

    memcpy(m_shared->frame->pixels, bgraPixels, copyBytes);

    // Increment frame index LAST so MirrorCam.dll sees a complete frame
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&m_shared->frame->frameIndex));

    // Signal MirrorCam.dll that a new frame is ready
    if (m_shared->hFrameReady) SetEvent(m_shared->hFrameReady);

    m_frames.fetch_add(1, std::memory_order_relaxed);
}

void VCamBridge::pushFrameGPU(ID3D12Resource* /*renderTarget*/,
                               uint32_t /*width*/, uint32_t /*height*/) {
    // Full implementation: GPU readback via staging texture + GPU fence
    // ID3D12Device::CreateCommittedResource (staging) →
    // CopyTextureRegion → ExecuteCommandLists → fence wait →
    // Map staging → pushFrame(pixels, ...)
    AURA_LOG_TRACE("VCamBridge", "GPU readback path — Phase 9.");
}

void VCamBridge::shutdown() {
    stop();
    if (m_shared->frame)      { UnmapViewOfFile(m_shared->frame); m_shared->frame = nullptr; }
    if (m_shared->hMapping)   { CloseHandle(m_shared->hMapping);  m_shared->hMapping = nullptr; }
    if (m_shared->hFrameReady){ CloseHandle(m_shared->hFrameReady); m_shared->hFrameReady = nullptr; }
}

} // namespace aura
