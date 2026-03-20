// =============================================================================
// SoftwareVideoDecoder.cpp -- FFmpeg software H.265/H.264 decoder
// =============================================================================

#include "../pch.h"
#include "SoftwareVideoDecoder.h"
#include "VideoDecoder.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <stdexcept>
#include <format>

namespace aura {

struct SoftwareVideoDecoder::FFmpegState {
    const AVCodec* codec{nullptr};
    AVCodecContext* ctx{nullptr};
    AVFrame* frame{nullptr};
    AVPacket* packet{nullptr};
};

// -----------------------------------------------------------------------------
SoftwareVideoDecoder::SoftwareVideoDecoder(ID3D12Device* device, VideoCodec codec)
    : m_device(device)
    , m_codec(codec)
    , m_ffmpeg(std::make_unique<FFmpegState>()) {}

// -----------------------------------------------------------------------------
SoftwareVideoDecoder::~SoftwareVideoDecoder() {
    shutdown();
}

// -----------------------------------------------------------------------------
void SoftwareVideoDecoder::init() {
    std::string codecName;
    AVCodecID codecId;
    
    switch (m_codec) {
        case VideoCodec::H264:
            codecName = "H.264/AVC";
            codecId = AV_CODEC_ID_H264;
            break;
        case VideoCodec::H265:
            codecName = "H.265/HEVC";
            codecId = AV_CODEC_ID_HEVC;
            break;
        case VideoCodec::AV1:
            codecName = "AV1";
            codecId = AV_CODEC_ID_AV1;
            break;
        default:
            throw std::runtime_error("Unknown codec");
    }
    
    AURA_LOG_INFO("SoftwareVideoDecoder", "Initialising {} software decoder (FFmpeg)...", codecName);

    // Find decoder
    m_ffmpeg->codec = avcodec_find_decoder(codecId);
    if (!m_ffmpeg->codec) {
        throw std::runtime_error(std::format(
            "SoftwareVideoDecoder: {} decoder not found", codecName));
    }

    // Allocate context
    m_ffmpeg->ctx = avcodec_alloc_context3(m_ffmpeg->codec);
    if (!m_ffmpeg->ctx) {
        throw std::runtime_error("SoftwareVideoDecoder: avcodec_alloc_context3 failed");
    }

    // Open codec
    if (avcodec_open2(m_ffmpeg->ctx, m_ffmpeg->codec, nullptr) < 0) {
        avcodec_free_context(&m_ffmpeg->ctx);
        throw std::runtime_error(std::format(
            "SoftwareVideoDecoder: avcodec_open2 failed for {}", codecName));
    }

    // Allocate frame and packet
    m_ffmpeg->frame = av_frame_alloc();
    m_ffmpeg->packet = av_packet_alloc();

    m_initialised.store(true);
    AURA_LOG_INFO("SoftwareVideoDecoder", "{} software decoder ready.", codecName);
}

// -----------------------------------------------------------------------------
void SoftwareVideoDecoder::shutdown() {
    if (!m_initialised.exchange(false)) return;

    AURA_LOG_INFO("SoftwareVideoDecoder", "Shutting down. Decoded: {} frames, Errors: {}",
                  m_decoded.load(), m_errors.load());

    if (m_ffmpeg->frame) {
        av_frame_free(&m_ffmpeg->frame);
    }
    if (m_ffmpeg->packet) {
        av_packet_free(&m_ffmpeg->packet);
    }
    if (m_ffmpeg->ctx) {
        avcodec_free_context(&m_ffmpeg->ctx);
    }
}

// -----------------------------------------------------------------------------
bool SoftwareVideoDecoder::submitNAL(const std::vector<uint8_t>& annexBData,
                                      bool isKeyframe,
                                      uint64_t presentationTimeUs) {
    if (!m_initialised.load()) return false;
    if (annexBData.empty()) return false;

    // Set packet data
    m_ffmpeg->packet->data = const_cast<uint8_t*>(annexBData.data());
    m_ffmpeg->packet->size = static_cast<int>(annexBData.size());
    m_ffmpeg->packet->pts = presentationTimeUs;

    // Send packet to decoder
    int ret = avcodec_send_packet(m_ffmpeg->ctx, m_ffmpeg->packet);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN)) {
            AURA_LOG_WARN("SoftwareVideoDecoder", "avcodec_send_packet failed: {}", ret);
            m_errors++;
        }
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
void SoftwareVideoDecoder::processOutput() {
    if (!m_initialised.load()) return;

    while (true) {
        int ret = avcodec_receive_frame(m_ffmpeg->ctx, m_ffmpeg->frame);
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // Need more input
        }
        
        if (ret < 0) {
            AURA_LOG_WARN("SoftwareVideoDecoder", "avcodec_receive_frame failed: {}", ret);
            m_errors++;
            break;
        }

        // Got a frame!
        m_decoded++;
        
        if (m_decoded % 30 == 0 || m_decoded <= 5) {
            AURA_LOG_INFO("SoftwareVideoDecoder", "Decoded frame #{}: {}x{} format={}", 
                          m_decoded.load(), m_ffmpeg->frame->width, m_ffmpeg->frame->height,
                          m_ffmpeg->frame->format);
        }

        // TODO: Convert YUV to NV12 texture and call m_frameCallback
        // For now, just count frames to verify decoder works
        
        av_frame_unref(m_ffmpeg->frame);
    }
}

// -----------------------------------------------------------------------------
void SoftwareVideoDecoder::setFrameCallback(FrameCallback cb) {
    m_frameCallback = std::move(cb);
}

// -----------------------------------------------------------------------------
void SoftwareVideoDecoder::resetState() {
    if (m_ffmpeg->ctx) {
        avcodec_flush_buffers(m_ffmpeg->ctx);
    }
}

} // namespace aura
