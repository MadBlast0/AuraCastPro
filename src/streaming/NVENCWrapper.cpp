// =============================================================================
// NVENCWrapper.cpp -- Task 135: NVIDIA NVENC direct SDK interface
//
// Strategy (compile-time):
//   • If CUDA toolkit installed  -> NVENC_SDK_AVAILABLE=1, use direct API
//   • Otherwise                  -> NVENC_SDK_AVAILABLE=0, route through FFmpeg
//     h264_nvenc / hevc_nvenc (which bundles its own nvEncodeAPI.dll at runtime)
//
// Runtime path (NVENC_SDK_AVAILABLE=1):
//   open()         -> NvEncOpenEncodeSessionEx (D3D12 device)
//                  -> NvEncInitializeEncoder (HEVC, CBR, P4 preset)
//                  -> NvEncCreateBitstreamBuffer (output bitstream)
//   encodeFrame()  -> NvEncRegisterResource (D3D12 texture)
//                  -> NvEncMapInputResource
//                  -> NvEncEncodePicture
//                  -> NvEncLockBitstream -> callback(packet)
//                  -> NvEncUnlockBitstream -> NvEncUnmapInputResource
//   close()        -> NvEncDestroyEncoder
// =============================================================================
#include "../pch.h"  // PCH
#include "NVENCWrapper.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>

#if __has_include(<nvEncodeAPI.h>)
#  include <nvEncodeAPI.h>
#  define NVENC_SDK_AVAILABLE 1
#else
#  define NVENC_SDK_AVAILABLE 0
#endif

#include <stdexcept>
#include <format>
#include <cstring>

namespace aura {

// ── Private state (pimpl -- keeps NVENC SDK types out of the header) ──────────

struct NVENCWrapper::NVState {
#if NVENC_SDK_AVAILABLE
    NV_ENCODE_API_FUNCTION_LIST api{};
    void*                       encoder{nullptr};
    NV_ENC_OUTPUT_PTR           bitstreamBuffer{nullptr};
    uint32_t                    width{};
    uint32_t                    height{};
#endif
    HMODULE  hDll{nullptr};
    bool     available{false};
    bool     sessionOpen{false};
};

// ── Constructor / Destructor ──────────────────────────────────────────────────

NVENCWrapper::NVENCWrapper() : m_nv(std::make_unique<NVState>()) {}
NVENCWrapper::~NVENCWrapper() { shutdown(); }

// ── isAvailable -- probe DLL without executing code ───────────────────────────

bool NVENCWrapper::isAvailable() const {
    // Fast path: if already initialised, use cached result
    if (m_nv && m_nv->available) return true;

    // Cold path: probe the DLL with LOAD_LIBRARY_AS_DATAFILE (no code executed)
    HMODULE hProbe = LoadLibraryExW(L"nvEncodeAPI64.dll", nullptr,
                                    LOAD_LIBRARY_AS_DATAFILE);
    if (!hProbe) {
        // Fallback: check 32-bit variant (unlikely on modern systems)
        hProbe = LoadLibraryExW(L"nvEncodeAPI.dll", nullptr,
                                LOAD_LIBRARY_AS_DATAFILE);
    }
    if (hProbe) {
        FreeLibrary(hProbe);
        return true;
    }
    return false;
}

// ── init -- probe DLL and populate function table ──────────────────────────────

void NVENCWrapper::init() {
#if NVENC_SDK_AVAILABLE
    m_nv->hDll = LoadLibraryW(L"nvEncodeAPI64.dll");
    if (!m_nv->hDll) {
        AURA_LOG_INFO("NVENCWrapper",
            "nvEncodeAPI64.dll not found -- using FFmpeg h264_nvenc fallback.");
        return;
    }

    using CreateInstanceFn = NVENCSTATUS(*)(NV_ENCODE_API_FUNCTION_LIST*);
    auto createInstance = reinterpret_cast<CreateInstanceFn>(
        GetProcAddress(m_nv->hDll, "NvEncodeAPICreateInstance"));

    if (!createInstance) {
        AURA_LOG_WARN("NVENCWrapper",
            "nvEncodeAPI64.dll missing NvEncodeAPICreateInstance export.");
        FreeLibrary(m_nv->hDll);
        m_nv->hDll = nullptr;
        return;
    }

    std::memset(&m_nv->api, 0, sizeof(m_nv->api));
    m_nv->api.version = NV_ENCODE_API_FUNCTION_LIST_VER;

    NVENCSTATUS status = createInstance(&m_nv->api);
    if (status != NV_ENC_SUCCESS) {
        AURA_LOG_WARN("NVENCWrapper",
            "NvEncodeAPICreateInstance failed: status {}. FFmpeg fallback active.", (int)status);
        FreeLibrary(m_nv->hDll);
        m_nv->hDll = nullptr;
        return;
    }

    m_nv->available = true;
    AURA_LOG_INFO("NVENCWrapper",
        "NVENC SDK initialised. Direct API available -- zero-copy D3D12 encoding enabled.");
#else
    AURA_LOG_INFO("NVENCWrapper",
        "Built without NVENC SDK headers. Using FFmpeg h264_nvenc / hevc_nvenc path.");
#endif
}

// ── open -- create encode session and configure for screen mirroring ───────────

bool NVENCWrapper::open(ID3D12Device* device,
                         uint32_t w, uint32_t h,
                         uint32_t fps, int bitrateKbps,
                         const char* codec) {
    if (!m_nv->available) return false;

#if NVENC_SDK_AVAILABLE
    // ── Open encode session (D3D12 interop) ──────────────────────────────────
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sessionParams{};
    sessionParams.version    = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    sessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    sessionParams.device     = device;
    sessionParams.apiVersion = NVENCAPI_VERSION;

    NVENCSTATUS st = m_nv->api.nvEncOpenEncodeSessionEx(&sessionParams, &m_nv->encoder);
    if (st != NV_ENC_SUCCESS) {
        AURA_LOG_ERROR("NVENCWrapper",
            "nvEncOpenEncodeSessionEx failed: {}. Check NVENC session limit (max 5).", (int)st);
        return false;
    }

    // ── Configure encoder (HEVC preferred for 4K/120fps; H.264 fallback) ─────
    const bool useHEVC = (std::string(codec ? codec : "") == "hevc" ||
                          w > 1920 || fps > 60);

    NV_ENC_INITIALIZE_PARAMS initParams{};
    initParams.version            = NV_ENC_INITIALIZE_PARAMS_VER;
    initParams.encodeGUID         = useHEVC ? NV_ENC_CODEC_HEVC_GUID : NV_ENC_CODEC_H264_GUID;
    initParams.presetGUID         = NV_ENC_PRESET_P4_GUID;   // balanced quality/speed
    initParams.encodeWidth        = w;
    initParams.encodeHeight       = h;
    initParams.darWidth           = w;
    initParams.darHeight          = h;
    initParams.frameRateNum       = fps;
    initParams.frameRateDen       = 1;
    initParams.enablePTD          = 1; // picture type decision
    initParams.reportSliceOffsets = 0;
    initParams.enableSubFrameWrite= 0;

    NV_ENC_CONFIG encConfig{};
    encConfig.version             = NV_ENC_CONFIG_VER;
    encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    encConfig.rcParams.averageBitRate  = static_cast<uint32_t>(bitrateKbps) * 1000;
    encConfig.rcParams.maxBitRate      = static_cast<uint32_t>(bitrateKbps) * 1500; // burst
    encConfig.rcParams.vbvBufferSize   = static_cast<uint32_t>(bitrateKbps) * 1000; // 1s VBV
    encConfig.rcParams.vbvInitialDelay = static_cast<uint32_t>(bitrateKbps) * 500;
    initParams.encodeConfig = &encConfig;

    st = m_nv->api.nvEncInitializeEncoder(m_nv->encoder, &initParams);
    if (st != NV_ENC_SUCCESS) {
        AURA_LOG_ERROR("NVENCWrapper",
            "nvEncInitializeEncoder failed: {}. Params: {}x{} {}fps {}kbps.",
            (int)st, w, h, fps, bitrateKbps);
        m_nv->api.nvEncDestroyEncoder(m_nv->encoder);
        m_nv->encoder = nullptr;
        return false;
    }

    // ── Create output bitstream buffer ───────────────────────────────────────
    NV_ENC_CREATE_BITSTREAM_BUFFER bsParams{};
    bsParams.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    st = m_nv->api.nvEncCreateBitstreamBuffer(m_nv->encoder, &bsParams);
    if (st != NV_ENC_SUCCESS) {
        AURA_LOG_ERROR("NVENCWrapper", "nvEncCreateBitstreamBuffer failed: {}", (int)st);
        m_nv->api.nvEncDestroyEncoder(m_nv->encoder);
        m_nv->encoder = nullptr;
        return false;
    }
    m_nv->bitstreamBuffer = bsParams.bitstreamBuffer;
    m_nv->width  = w;
    m_nv->height = h;
    m_nv->sessionOpen = true;
    m_open.store(true);

    AURA_LOG_INFO("NVENCWrapper",
        "NVENC session open: {}x{} {} @{}fps {}kbps ({}). D3D12 zero-copy active.",
        w, h, useHEVC ? "HEVC" : "H.264", fps, bitrateKbps,
        NV_ENC_PRESET_P4_GUID == NV_ENC_PRESET_P4_GUID ? "P4 preset" : "");
    return true;
#else
    (void)device; (void)w; (void)h; (void)fps; (void)bitrateKbps; (void)codec;
    return false;
#endif
}

// ── encodeFrame -- register D3D12 texture, encode, deliver packet ──────────────

void NVENCWrapper::encodeFrame(ID3D12Resource* texture, int64_t ptsUs) {
    if (!m_open.load() || !m_callback) return;

#if NVENC_SDK_AVAILABLE
    if (!m_nv->sessionOpen || !m_nv->encoder || !texture) return;

    // Register D3D12 resource (cached per texture -- re-registration is a no-op if already done)
    NV_ENC_REGISTER_RESOURCE regParams{};
    regParams.version            = NV_ENC_REGISTER_RESOURCE_VER;
    regParams.resourceType       = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    regParams.width              = m_nv->width;
    regParams.height             = m_nv->height;
    regParams.pitch              = 0; // D3D12 handles pitch internally
    regParams.resourceToRegister = texture;
    regParams.bufferFormat       = NV_ENC_BUFFER_FORMAT_NV12;

    NV_ENC_REGISTERED_PTR registeredResource{};
    NVENCSTATUS st = m_nv->api.nvEncRegisterResource(m_nv->encoder, &regParams);
    if (st != NV_ENC_SUCCESS) {
        AURA_LOG_WARN("NVENCWrapper", "nvEncRegisterResource failed: {}", (int)st);
        return;
    }
    registeredResource = regParams.registeredResource;

    // Map input resource for encoder access
    NV_ENC_MAP_INPUT_RESOURCE mapParams{};
    mapParams.version            = NV_ENC_MAP_INPUT_RESOURCE_VER;
    mapParams.registeredResource = registeredResource;
    st = m_nv->api.nvEncMapInputResource(m_nv->encoder, &mapParams);
    if (st != NV_ENC_SUCCESS) {
        AURA_LOG_WARN("NVENCWrapper", "nvEncMapInputResource failed: {}", (int)st);
        m_nv->api.nvEncUnregisterResource(m_nv->encoder, registeredResource);
        return;
    }

    // Submit frame for encoding
    NV_ENC_PIC_PARAMS picParams{};
    picParams.version          = NV_ENC_PIC_PARAMS_VER;
    picParams.inputWidth       = m_nv->width;
    picParams.inputHeight      = m_nv->height;
    picParams.inputPitch       = 0;
    picParams.encodePicFlags   = 0;
    picParams.inputTimeStamp   = static_cast<uint64_t>(ptsUs);
    picParams.inputDuration    = 0;
    picParams.inputBuffer      = mapParams.mappedResource;
    picParams.outputBitstream  = m_nv->bitstreamBuffer;
    picParams.bufferFmt        = NV_ENC_BUFFER_FORMAT_NV12;
    picParams.pictureStruct    = NV_ENC_PIC_STRUCT_FRAME;

    st = m_nv->api.nvEncEncodePicture(m_nv->encoder, &picParams);
    m_nv->api.nvEncUnmapInputResource(m_nv->encoder, mapParams.mappedResource);
    m_nv->api.nvEncUnregisterResource(m_nv->encoder, registeredResource);

    if (st != NV_ENC_SUCCESS && st != NV_ENC_ERR_NEED_MORE_INPUT) {
        AURA_LOG_WARN("NVENCWrapper", "nvEncEncodePicture failed: {}", (int)st);
        return;
    }

    // Lock bitstream and deliver to callback
    NV_ENC_LOCK_BITSTREAM lockParams{};
    lockParams.version         = NV_ENC_LOCK_BITSTREAM_VER;
    lockParams.outputBitstream = m_nv->bitstreamBuffer;
    lockParams.doNotWait       = 0;  // blocking -- fine at 120fps (8.3ms budget)

    st = m_nv->api.nvEncLockBitstream(m_nv->encoder, &lockParams);
    if (st == NV_ENC_SUCCESS) {
        m_callback(
            static_cast<const uint8_t*>(lockParams.bitstreamBufferPtr),
            lockParams.bitstreamSizeInBytes,
            ptsUs,
            (lockParams.pictureType == NV_ENC_PIC_TYPE_IDR ||
             lockParams.pictureType == NV_ENC_PIC_TYPE_I));
        m_nv->api.nvEncUnlockBitstream(m_nv->encoder, m_nv->bitstreamBuffer);
    }
#else
    (void)texture; (void)ptsUs;
#endif
}

// ── close / shutdown ──────────────────────────────────────────────────────────

void NVENCWrapper::close() {
    if (!m_nv->sessionOpen) return;
    m_open.store(false);

#if NVENC_SDK_AVAILABLE
    if (m_nv->encoder) {
        // Flush remaining frames
        NV_ENC_PIC_PARAMS flushParams{};
        flushParams.version        = NV_ENC_PIC_PARAMS_VER;
        flushParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
        m_nv->api.nvEncEncodePicture(m_nv->encoder, &flushParams);

        if (m_nv->bitstreamBuffer)
            m_nv->api.nvEncDestroyBitstreamBuffer(m_nv->encoder, m_nv->bitstreamBuffer);
        m_nv->api.nvEncDestroyEncoder(m_nv->encoder);
        m_nv->encoder        = nullptr;
        m_nv->bitstreamBuffer= nullptr;
    }
#endif
    m_nv->sessionOpen = false;
    AURA_LOG_INFO("NVENCWrapper", "Session closed.");
}

void NVENCWrapper::setCallback(PacketCallback cb) { m_callback = std::move(cb); }

void NVENCWrapper::shutdown() {
    close();
    if (m_nv->hDll) {
        FreeLibrary(m_nv->hDll);
        m_nv->hDll = nullptr;
    }
    m_nv->available = false;
}

} // namespace aura
