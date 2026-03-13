#include "../pch.h"  // PCH
#include "QSVWrapper.h"
#include "../utils/Logger.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

QSVWrapper::QSVWrapper() = default;

QSVWrapper::~QSVWrapper() { shutdown(); }

bool QSVWrapper::isAvailable() {
    const AVCodec* c = avcodec_find_encoder_by_name("h264_qsv");
    if (!c) return false;
    AVCodecContext* ctx = avcodec_alloc_context3(c);
    if (!ctx) return false;
    ctx->width  = 1280; ctx->height = 720;
    ctx->time_base = { 1, 30 };
    ctx->pix_fmt   = AV_PIX_FMT_QSV;
    // Probe open -- will fail without Intel GPU but won't crash
    bool ok = (avcodec_open2(ctx, c, nullptr) >= 0);
    avcodec_free_context(&ctx);
    return ok;
}

bool QSVWrapper::init(int width, int height, int bitrateMbps, int fps) {
    m_width  = width;
    m_height = height;
    m_fps    = fps;

    const AVCodec* codec = avcodec_find_encoder_by_name("h264_qsv");
    if (!codec) {
        LOG_WARN("QSVWrapper: h264_qsv not available -- QSV unavailable");
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) return false;

    m_codecCtx->width       = width;
    m_codecCtx->height      = height;
    m_codecCtx->time_base   = { 1, fps };
    m_codecCtx->framerate   = { fps, 1 };
    m_codecCtx->bit_rate    = bitrateMbps * 1000LL * 1000LL;
    m_codecCtx->rc_max_rate = m_codecCtx->bit_rate;
    m_codecCtx->rc_buffer_size = m_codecCtx->bit_rate;
    m_codecCtx->pix_fmt     = AV_PIX_FMT_QSV;
    m_codecCtx->gop_size    = fps * 2;   // keyframe every 2s
    m_codecCtx->max_b_frames = 0;        // zero latency

    // QSV-specific options
    av_opt_set(m_codecCtx->priv_data, "preset",      "veryfast", 0);
    av_opt_set(m_codecCtx->priv_data, "look_ahead",  "0",        0);
    av_opt_set(m_codecCtx->priv_data, "rc_mode",     "cbr",      0);

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        LOG_ERROR("QSVWrapper: avcodec_open2 failed");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    m_frame = av_frame_alloc();
    m_frame->format = AV_PIX_FMT_NV12;
    m_frame->width  = width;
    m_frame->height = height;
    av_frame_get_buffer(m_frame, 0);

    m_pkt = av_packet_alloc();

    LOG_INFO("QSVWrapper: Intel QSV encoder initialised ({}x{} {}Mbps {}fps)",
             width, height, bitrateMbps, fps);
    return true;
}

bool QSVWrapper::encodeFrame(ID3D12Resource* /*frame*/) {
    if (!m_codecCtx || !m_frame) return false;
    // In production: GPU readback -> fill m_frame->data planes
    // For now: zero frame (black) to verify pipeline compiles
    av_frame_make_writable(m_frame);
    int ret = avcodec_send_frame(m_codecCtx, m_frame);
    if (ret < 0) return false;
    ret = avcodec_receive_packet(m_codecCtx, m_pkt);
    m_hasPacket = (ret >= 0);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
    return ret >= 0;
}

void QSVWrapper::getEncodedPacket(AVPacket* out) {
    if (!out || !m_hasPacket) return;
    av_packet_move_ref(out, m_pkt);
    m_hasPacket = false;
}

void QSVWrapper::drain() {
    if (!m_codecCtx) return;
    avcodec_send_frame(m_codecCtx, nullptr);
    while (avcodec_receive_packet(m_codecCtx, m_pkt) >= 0)
        av_packet_unref(m_pkt);
}

void QSVWrapper::shutdown() {
    drain();
    if (m_frame)    { av_frame_free(&m_frame);    m_frame    = nullptr; }
    if (m_pkt)      { av_packet_free(&m_pkt);     m_pkt      = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); m_codecCtx = nullptr; }
}
