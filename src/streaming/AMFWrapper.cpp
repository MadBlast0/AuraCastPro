// =============================================================================
// AMFWrapper.cpp -- AMD Advanced Media Framework (AMF) GPU encoder
//
// Strategy:
//   isAvailable()  -> probe amfrt64.dll with LOAD_LIBRARY_AS_DATAFILE
//   init()         -> open FFmpeg AVCodecContext for h264_amf or hevc_amf
//                    (FFmpeg ships its own amfrt64.dll bridge so we don't need
//                     the full AMD Media SDK at compile time)
//   encodeFrame()  -> send AVFrame to codec, retrieve AVPackets via callback
//   shutdown()     -> flush codec, free context
//
// Falls back transparently if amfrt64.dll is absent (e.g. Intel/NVIDIA only).
// =============================================================================
#include "../pch.h"  // PCH
#include "AMFWrapper.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d12va.h>
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static constexpr const char* kAmfDll = "amfrt64.dll";

// =============================================================================
// AMFWrapper
// =============================================================================

AMFWrapper::AMFWrapper()  = default;
AMFWrapper::~AMFWrapper() { shutdown(); }

// ── isAvailable -- probe DLL without executing code ────────────────────────────

bool AMFWrapper::isAvailable() {
    HMODULE h = LoadLibraryExA(kAmfDll, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    if (h) { FreeLibrary(h); return true; }
    return false;
}

// ── loadAMF / unloadAMF ───────────────────────────────────────────────────────

bool AMFWrapper::loadAMF() {
    m_hAmf = LoadLibraryA(kAmfDll);
    if (!m_hAmf) {
        AURA_LOG_WARN("AMFWrapper", "{} not found -- AMD AMF encoder unavailable.", kAmfDll);
        return false;
    }
    AURA_LOG_INFO("AMFWrapper", "amfrt64.dll loaded successfully.");
    return true;
}

void AMFWrapper::unloadAMF() {
    if (m_hAmf) { FreeLibrary(static_cast<HMODULE>(m_hAmf)); m_hAmf = nullptr; }
}

// ── init -- open FFmpeg AMF codec context ──────────────────────────────────────

bool AMFWrapper::init(int width, int height, int bitrateMbps, int fps) {
    if (!loadAMF()) return false;

    m_width      = width;
    m_height     = height;
    m_bitrateBps = bitrateMbps * 1'000'000;
    m_fps        = fps;

    // Select codec: prefer HEVC (h265_amf) for quality, fallback to h264_amf
    const char* codecNames[] = { "hevc_amf", "h264_amf" };
    const AVCodec* codec = nullptr;
    for (const char* name : codecNames) {
        codec = avcodec_find_encoder_by_name(name);
        if (codec) {
            AURA_LOG_INFO("AMFWrapper", "Using AMF codec: {}", name);
            break;
        }
    }
    if (!codec) {
        AURA_LOG_ERROR("AMFWrapper",
            "Neither hevc_amf nor h264_amf found in FFmpeg build.");
        unloadAMF();
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) {
        AURA_LOG_ERROR("AMFWrapper", "avcodec_alloc_context3 failed.");
        unloadAMF();
        return false;
    }

    // Configure codec parameters
    m_ctx->width       = width;
    m_ctx->height      = height;
    m_ctx->time_base   = { 1, fps };
    m_ctx->framerate   = { fps, 1 };
    m_ctx->bit_rate    = m_bitrateBps;
    m_ctx->rc_max_rate = m_bitrateBps;
    m_ctx->pix_fmt     = AV_PIX_FMT_NV12;
    m_ctx->gop_size    = fps * 2;    // keyframe every 2 seconds
    m_ctx->max_b_frames = 0;         // low latency: no B-frames

    // AMF-specific options for low-latency screen mirroring
    av_opt_set(m_ctx->priv_data, "usage",   "ultralowlatency", 0);
    av_opt_set(m_ctx->priv_data, "quality", "speed",           0);
    av_opt_set(m_ctx->priv_data, "rc",      "cbr",             0);

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        AURA_LOG_ERROR("AMFWrapper", "avcodec_open2 failed for {} encoder.", codec->name);
        avcodec_free_context(&m_ctx);
        m_ctx = nullptr;
        unloadAMF();
        return false;
    }

    m_packet = av_packet_alloc();
    AURA_LOG_INFO("AMFWrapper",
        "AMD AMF encoder ready: {}x{} {}Mbps {}fps ({})",
        width, height, bitrateMbps, fps, codec->name);
    return true;
}

// ── encodeFrame ───────────────────────────────────────────────────────────────

bool AMFWrapper::encodeFrame(ID3D12Resource* /*d3dFrame*/) {
    if (!m_ctx) return false;
    // In a full D3D12 AMF integration, the frame would be wrapped as an
    // AMFSurface via D3D12 interop.  Here we route through FFmpeg which manages
    // the surface wrapping internally via its amf bridge.
    // The caller is expected to call encodeAvFrame() with a populated AVFrame.
    return true;
}

bool AMFWrapper::encodeAvFrame(AVFrame* frame) {
    if (!m_ctx || !frame) return false;

    int ret = avcodec_send_frame(m_ctx, frame);
    if (ret < 0) {
        AURA_LOG_WARN("AMFWrapper", "avcodec_send_frame error: {}", ret);
        return false;
    }

    m_hasPacket = false;
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_ctx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            AURA_LOG_WARN("AMFWrapper", "avcodec_receive_packet error: {}", ret);
            break;
        }
        if (m_callback) {
            m_callback(m_packet->data, (size_t)m_packet->size,
                       m_packet->pts,
                       (m_packet->flags & AV_PKT_FLAG_KEY) != 0);
        }
        m_hasPacket = true;
        av_packet_unref(m_packet);
    }
    return true;
}

// ── getEncodedPacket ──────────────────────────────────────────────────────────

void AMFWrapper::getEncodedPacket(AVPacket* out) {
    if (!m_ctx || !out) return;
    int ret = avcodec_receive_packet(m_ctx, out);
    if (ret < 0) out->size = 0;
}

// ── drain -- flush remaining packets ──────────────────────────────────────────

void AMFWrapper::drain() {
    if (!m_ctx) return;
    AURA_LOG_DEBUG("AMFWrapper", "Draining encoder...");
    avcodec_send_frame(m_ctx, nullptr);  // flush
    while (true) {
        int ret = avcodec_receive_packet(m_ctx, m_packet);
        if (ret == AVERROR_EOF || ret < 0) break;
        if (m_callback) {
            m_callback(m_packet->data, (size_t)m_packet->size,
                       m_packet->pts,
                       (m_packet->flags & AV_PKT_FLAG_KEY) != 0);
        }
        av_packet_unref(m_packet);
    }
}

// ── shutdown ──────────────────────────────────────────────────────────────────

void AMFWrapper::shutdown() {
    drain();
    if (m_packet)  { av_packet_free(&m_packet); }
    if (m_ctx)     { avcodec_free_context(&m_ctx); }
    m_encoder = nullptr;
    m_context = nullptr;
    m_factory = nullptr;
    unloadAMF();
    AURA_LOG_INFO("AMFWrapper", "Shutdown complete.");
}
