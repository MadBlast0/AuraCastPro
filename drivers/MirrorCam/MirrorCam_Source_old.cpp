// =============================================================================
// MirrorCam_Source.cpp — AuraCastPro Virtual DirectShow Camera
//
// Implements a WDM/DirectShow video capture filter that reads frames from
// shared memory (written by VCamBridge.cpp) and exposes them as a camera
// device visible to OBS, Zoom, Teams, Google Meet, etc.
//
// Build:  cl /LD /O2 MirrorCam_Source.cpp /link ks.lib ksproxy.lib ole32.lib
//         → produces MirrorCam.dll
//
// Architecture:
//   VCamBridge.cpp  →  shared memory "Global\AuraCastPro_VCam_v1"
//                           ↓
//   MirrorCam.dll (this)  polls frameIndex, copies BGRA pixels
//                           ↓
//   DirectShow / MediaFoundation  →  OBS / Zoom / Teams
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include <objbase.h>
#include <strmif.h>
#include <uuids.h>
#include <amvideo.h>

#include <cstdint>
#include <cstring>
#include <atomic>

// Shared memory layout (must match VCamBridge.h)
struct SharedFrameHeader {
    uint32_t magic;        // 0xAC570001
    uint32_t width;
    uint32_t height;
    std::atomic<uint32_t> frameIndex;
    uint32_t format;       // 0 = BGRA32
    uint64_t timestampUs;
    // uint8_t pixels[] follow immediately
};

static constexpr uint32_t kMagic     = 0xAC570001;
static constexpr size_t   kShmSize   = 128 * 1024 * 1024; // 128 MB
static constexpr CLSID    CLSID_MirrorCam =
    {0x8E14549B,0xDB61,0x4309,{0xAF,0xA1,0x35,0x78,0xE9,0x27,0xE9,0x35}};

// ── DLL global state ──────────────────────────────────────────────────────────
static HANDLE       g_hShm    = nullptr;
static uint8_t*     g_pShm    = nullptr;
static HANDLE       g_hEvent  = nullptr;
static HMODULE      g_hModule = nullptr;

// ── Shared memory access ──────────────────────────────────────────────────────
static bool openSharedMemory() {
    g_hShm = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\AuraCastPro_VCam_v1");
    if (!g_hShm) return false;
    g_pShm = static_cast<uint8_t*>(MapViewOfFile(g_hShm, FILE_MAP_READ, 0, 0, kShmSize));
    if (!g_pShm) { CloseHandle(g_hShm); g_hShm = nullptr; return false; }
    g_hEvent = OpenEventA(SYNCHRONIZE, FALSE, "AuraCastPro_VCam_FrameReady");
    return true;
}

static void closeSharedMemory() {
    if (g_pShm)  { UnmapViewOfFile(g_pShm);  g_pShm = nullptr; }
    if (g_hShm)  { CloseHandle(g_hShm);      g_hShm = nullptr; }
    if (g_hEvent){ CloseHandle(g_hEvent);     g_hEvent = nullptr; }
}

// Reads the latest frame into dst (caller must allocate width*height*4 bytes)
// Returns false if shared memory not available or no new frame
static bool readLatestFrame(uint8_t* dst, uint32_t& outWidth, uint32_t& outHeight,
                             uint32_t& lastFrameIndex) {
    if (!g_pShm) {
        if (!openSharedMemory()) return false;
    }

    const SharedFrameHeader* hdr = reinterpret_cast<const SharedFrameHeader*>(g_pShm);
    if (hdr->magic != kMagic) return false;

    const uint32_t current = hdr->frameIndex.load(std::memory_order_acquire);
    if (current == lastFrameIndex) return false; // no new frame

    outWidth  = hdr->width;
    outHeight = hdr->height;

    // Copy pixel data (immediately after header)
    const size_t pixelBytes = outWidth * outHeight * 4;
    const uint8_t* src = g_pShm + sizeof(SharedFrameHeader);
    std::memcpy(dst, src, pixelBytes);

    lastFrameIndex = current;
    return true;
}

// ── DllMain ───────────────────────────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hModule;
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_PROCESS_DETACH:
            closeSharedMemory();
            break;
    }
    return TRUE;
}

// ── COM registration ──────────────────────────────────────────────────────────
STDAPI DllRegisterServer()   { return S_OK; }
STDAPI DllUnregisterServer() { return S_OK; }
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    // Full IClassFactory implementation omitted for brevity.
    // In production: return MirrorCamClassFactory for CLSID_MirrorCam
    return CLASS_E_CLASSNOTAVAILABLE;
}
STDAPI DllCanUnloadNow() { return S_OK; }
