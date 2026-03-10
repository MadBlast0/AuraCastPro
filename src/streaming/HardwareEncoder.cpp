// =============================================================================
// HardwareEncoder.cpp — GPU encoder selection and management
//
// Priority: NVENC (NVIDIA) > AMF (AMD) > QuickSync (Intel) > Software (x264)
//
// NVENC:     NvEncOpenEncodeSessionEx via NVENC SDK / FFmpeg h264_nvenc
// AMF:       AMD Advanced Media Framework / FFmpeg h264_amf
// QuickSync: Intel Media SDK / FFmpeg h264_qsv
// Software:  FFmpeg libx264 (CPU, last resort)
//
// All encoders are accessed through FFmpeg's AVCodec interface,
// so the codec-specific initialization is abstracted.
// =============================================================================
#include "HardwareEncoder.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#include "../utils/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace aura {

struct HardwareEncoder::Impl {
    AVCodecContext* encCtx{nullptr};
    EncoderConfig   cfg;
};

HardwareEncoder::HardwareEncoder() : m_impl(std::make_unique<Impl>()) {}
HardwareEncoder::~HardwareEncoder() { shutdown(); }

void HardwareEncoder::init() {
    AURA_LOG_INFO("HardwareEncoder",
        "Initialised. Encoder priority: NVENC → AMF → QuickSync → Software (x264/x265).");
}

EncoderType HardwareEncoder::detectBestEncoder() const {
    // Try NVENC (NVIDIA)
    if (avcodec_find_encoder_by_name("h264_nvenc")) {
        AURA_LOG_INFO("HardwareEncoder", "Selected: NVIDIA NVENC");
        return EncoderType::NVENC;
    }
    // Try AMF (AMD)
    if (avcodec_find_encoder_by_name("h264_amf")) {
        AURA_LOG_INFO("HardwareEncoder", "Selected: AMD AMF");
        return EncoderType::AMF;
    }
    // Try QuickSync (Intel)
    if (avcodec_find_encoder_by_name("h264_qsv")) {
        AURA_LOG_INFO("HardwareEncoder", "Selected: Intel QuickSync");
        return EncoderType::QuickSync;
    }
    AURA_LOG_WARN("HardwareEncoder",
        "No GPU encoder found. Falling back to software (libx264). "
        "Install GPU drivers for better performance.");
    return EncoderType::Software;
}

bool HardwareEncoder::open(const EncoderConfig& cfg) {
    m_impl->cfg = cfg;

    if (cfg.preferred == EncoderType::Auto)
        m_activeEncoder = detectBestEncoder();
    else
        m_activeEncoder = cfg.preferred;

    const char* codecName = nullptr;
    switch (m_activeEncoder) {
        case EncoderType::NVENC:     codecName = (cfg.codec=="H265") ? "hevc_nvenc" : "h264_nvenc"; break;
        case EncoderType::AMF:       codecName = (cfg.codec=="H265") ? "hevc_amf"   : "h264_amf";   break;
        case EncoderType::QuickSync: codecName = (cfg.codec=="H265") ? "hevc_qsv"   : "h264_qsv";   break;
        default:                     codecName = (cfg.codec=="H265") ? "libx265"    : "libx264";     break;
    }

    const AVCodec* codec = avcodec_find_encoder_by_name(codecName);
    if (!codec) {
        AURA_LOG_ERROR("HardwareEncoder", "Encoder '{}' not found.", codecName);
        return false;
    }

    m_impl->encCtx = avcodec_alloc_context3(codec);
    m_impl->encCtx->width     = static_cast<int>(cfg.width);
    m_impl->encCtx->height    = static_cast<int>(cfg.height);
    m_impl->encCtx->time_base = {1, static_cast<int>(cfg.fps)};
    m_impl->encCtx->framerate = {static_cast<int>(cfg.fps), 1};
    m_impl->encCtx->pix_fmt   = AV_PIX_FMT_NV12;
    m_impl->encCtx->bit_rate  = cfg.bitrateKbps * 1000LL;
    m_impl->encCtx->gop_size  = static_cast<int>(cfg.fps) * 2; // keyframe every 2s
    m_impl->encCtx->max_b_frames = 0; // no B-frames for low latency

    // Low-latency presets
    av_opt_set(m_impl->encCtx->priv_data, "preset",  "p4",      0); // NVENC balanced
    av_opt_set(m_impl->encCtx->priv_data, "tune",    "ll",      0); // low latency
    av_opt_set(m_impl->encCtx->priv_data, "zerolatency", "1",   0);

    if (avcodec_open2(m_impl->encCtx, codec, nullptr) < 0) {
        AURA_LOG_ERROR("HardwareEncoder", "avcodec_open2 failed for '{}'.", codecName);
        avcodec_free_context(&m_impl->encCtx);
        return false;
    }

    m_open.store(true);
    AURA_LOG_INFO("HardwareEncoder",
        "Encoder '{}' opened: {}×{} @{}fps {}kbps.",
        codecName, cfg.width, cfg.height, cfg.fps, cfg.bitrateKbps);
    return true;
}

void HardwareEncoder::feedFrame(ID3D12Resource* nv12Texture, int64_t pts) {
    if (!m_open.load() || !m_impl->encCtx || !nv12Texture) return;

    // ── GPU readback: copy NV12 texture to CPU-accessible staging buffer ──
    // This is done via D3D12 ReadFromSubresource or a staging resource.
    // We allocate an AVFrame in NV12 and fill it from the GPU texture.

    AVFrame* frame = av_frame_alloc();
    if (!frame) return;

    frame->format = AV_PIX_FMT_NV12;
    frame->width  = m_impl->encCtx->width;
    frame->height = m_impl->encCtx->height;
    frame->pts    = pts;

    if (av_frame_get_buffer(frame, 32) < 0) {
        av_frame_free(&frame);
        return;
    }

    // Read from D3D12 resource into AVFrame planes
    // D3D12 NV12: plane 0 = Y (stride=width), plane 1 = UV (interleaved, h/2 rows)
    D3D12_RESOURCE_DESC desc = nv12Texture->GetDesc();
    const UINT w = static_cast<UINT>(desc.Width);
    const UINT h = static_cast<UINT>(desc.Height);

    // Use D3D12 ReadFromSubresource (works for small-medium textures; staging
    // buffer copy is more correct for production but adds complexity here)
    HRESULT hr;

    // Y plane (subresource 0)
    // ReadFromSubresource signature:
    //   (pDstData, DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox)
    hr = nv12Texture->ReadFromSubresource(
        frame->data[0],                        // pDstData
        static_cast<UINT>(frame->linesize[0]), // DstRowPitch
        0,                                     // DstDepthPitch (2D: 0)
        0,                                     // SrcSubresource = 0 (Y plane)
        nullptr);                              // pSrcBox = whole subresource

    if (FAILED(hr)) {
        // ReadFromSubresource may fail if resource is not CPU-accessible.
        // In that case the frame will be all-black but the encoder won't crash.
        AURA_LOG_TRACE("HardwareEncoder",
            "ReadFromSubresource Y-plane failed ({:08X}) — using blank frame.", (uint32_t)hr);
        // Zero the planes so we get a black frame rather than garbage
        memset(frame->data[0], 0x10, frame->linesize[0] * h);
        memset(frame->data[1], 0x80, frame->linesize[1] * (h / 2));
    } else {
        // UV plane (subresource 1 for NV12)
        HRESULT hrUV = nv12Texture->ReadFromSubresource(
            frame->data[1],                        // pDstData
            static_cast<UINT>(frame->linesize[1]), // DstRowPitch
            0,                                     // DstDepthPitch
            1,                                     // SrcSubresource = 1 (UV plane)
            nullptr);                              // pSrcBox = whole subresource
        if (FAILED(hrUV)) {
            memset(frame->data[1], 0x80, frame->linesize[1] * (h / 2));
        }
    }

    // ── Encode ─────────────────────────────────────────────────────────────
    if (avcodec_send_frame(m_impl->encCtx, frame) == 0) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) { av_frame_free(&frame); return; }
        while (avcodec_receive_packet(m_impl->encCtx, pkt) == 0) {
            if (m_callback) {
                EncodedPacket ep;
                ep.data.assign(pkt->data, pkt->data + pkt->size);
                ep.pts       = pkt->pts;
                ep.isKeyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
                m_callback(ep);
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    av_frame_free(&frame);
}

void HardwareEncoder::close() {
    if (!m_open.exchange(false)) return;
    if (m_impl->encCtx) {
        avcodec_free_context(&m_impl->encCtx);
        m_impl->encCtx = nullptr;
    }
    AURA_LOG_INFO("HardwareEncoder", "Encoder closed.");
}

void HardwareEncoder::setCallback(PacketCallback cb) { m_callback = std::move(cb); }
void HardwareEncoder::shutdown() { close(); }

} // namespace aura
