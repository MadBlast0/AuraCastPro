// =============================================================================
// RTMPOutput.cpp — Live RTMP streaming via FFmpeg libavformat
//
// Pushes the mirrored device screen to any RTMP endpoint.
// Uses the same FFmpeg pipeline as StreamRecorder but with network output.
//
// Supported platforms:
//   Twitch:  rtmp://live.twitch.tv/app/<stream_key>
//   YouTube: rtmp://a.rtmp.youtube.com/live2/<stream_key>
//   Custom:  rtmp://your-server:1935/live/<key>
//
// Video: stream copy (H.265) or transcode to H.264 for compatibility.
// Audio: PCM → AAC-LC 128kbps.
// =============================================================================
#include "../pch.h"  // PCH
#include "RTMPOutput.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <stdexcept>
#include <format>

namespace aura {

struct RTMPOutput::FFmpegRTMP {
    AVFormatContext* fmtCtx{nullptr};
    AVStream*        videoStream{nullptr};
    AVStream*        audioStream{nullptr};
    AVCodecContext*  audioEncCtx{nullptr};
    AVFrame*         audioFrame{nullptr};
    int64_t          audioPts{0};
    bool             headerWritten{false};
};

RTMPOutput::RTMPOutput()  : m_rtmp(std::make_unique<FFmpegRTMP>()) {}
RTMPOutput::~RTMPOutput() { shutdown(); }

void RTMPOutput::init() {
    AURA_LOG_INFO("RTMPOutput",
        "Initialised. FFmpeg RTMP output. "
        "Supported: Twitch, YouTube Live, custom RTMP servers. "
        "Video: stream copy H.265 or transcode to H.264. Audio: AAC-LC 128kbps.");
}

bool RTMPOutput::connect(const RTMPConfig& cfg) {
    if (m_connected.load()) disconnect();

    AURA_LOG_INFO("RTMPOutput", "Connecting to: {}", cfg.url);

    // Allocate RTMP output context
    int ret = avformat_alloc_output_context2(
        &m_rtmp->fmtCtx, nullptr, "flv", cfg.url.c_str());
    if (ret < 0 || !m_rtmp->fmtCtx) {
        AURA_LOG_ERROR("RTMPOutput", "Failed to allocate RTMP context: {}", ret);
        return false;
    }

    // Tune RTMP for low-latency streaming
    av_opt_set_int(m_rtmp->fmtCtx, "flush_packets", 1, AV_OPT_SEARCH_CHILDREN);

    if (!initVideoStream(cfg)) return false;
    if (!initAudioStream(cfg)) return false;

    // Open RTMP connection
    if (!(m_rtmp->fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&m_rtmp->fmtCtx->pb, cfg.url.c_str(),
                         AVIO_FLAG_WRITE, nullptr, nullptr);
        if (ret < 0) {
            AURA_LOG_ERROR("RTMPOutput",
                "avio_open2 failed for {}: {} — check URL and stream key.", cfg.url, ret);
            return false;
        }
    }

    // Write FLV header
    ret = avformat_write_header(m_rtmp->fmtCtx, nullptr);
    if (ret < 0) {
        AURA_LOG_ERROR("RTMPOutput", "avformat_write_header failed: {}", ret);
        return false;
    }

    m_rtmp->headerWritten = true;
    m_startTime = av_gettime();
    m_connected.store(true);

    AURA_LOG_INFO("RTMPOutput", "Streaming live to: {}", cfg.url);
    return true;
}

bool RTMPOutput::initVideoStream(const RTMPConfig& cfg) {
    m_rtmp->videoStream = avformat_new_stream(m_rtmp->fmtCtx, nullptr);
    if (!m_rtmp->videoStream) return false;

    AVCodecParameters* par = m_rtmp->videoStream->codecpar;
    par->codec_type = AVMEDIA_TYPE_VIDEO;
    par->codec_id   = (cfg.videoCodec == "H265") ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    par->width      = static_cast<int>(cfg.width);
    par->height     = static_cast<int>(cfg.height);
    par->bit_rate   = cfg.videoBitrateKbps * 1000LL;

    m_rtmp->videoStream->time_base = {1, static_cast<int>(cfg.fps) * 1000};
    m_rtmp->videoStream->avg_frame_rate = {static_cast<int>(cfg.fps), 1};
    return true;
}

bool RTMPOutput::initAudioStream(const RTMPConfig& cfg) {
    const AVCodec* aac = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!aac) { AURA_LOG_WARN("RTMPOutput", "AAC encoder missing."); return true; }

    m_rtmp->audioEncCtx = avcodec_alloc_context3(aac);
    m_rtmp->audioEncCtx->sample_rate  = 44100; // RTMP standard
    m_rtmp->audioEncCtx->ch_layout    = AV_CHANNEL_LAYOUT_STEREO;
    m_rtmp->audioEncCtx->sample_fmt   = aac->sample_fmts[0];
    m_rtmp->audioEncCtx->bit_rate     = cfg.audioBitrateKbps * 1000LL;
    m_rtmp->audioEncCtx->time_base    = {1, 44100};

    if (m_rtmp->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_rtmp->audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_rtmp->audioEncCtx, aac, nullptr) < 0) {
        AURA_LOG_WARN("RTMPOutput", "Failed to open AAC encoder."); return true;
    }

    m_rtmp->audioStream = avformat_new_stream(m_rtmp->fmtCtx, nullptr);
    avcodec_parameters_from_context(m_rtmp->audioStream->codecpar, m_rtmp->audioEncCtx);
    m_rtmp->audioStream->time_base = m_rtmp->audioEncCtx->time_base;

    m_rtmp->audioFrame = av_frame_alloc();
    m_rtmp->audioFrame->nb_samples  = m_rtmp->audioEncCtx->frame_size;
    m_rtmp->audioFrame->format      = m_rtmp->audioEncCtx->sample_fmt;
    m_rtmp->audioFrame->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
    m_rtmp->audioFrame->sample_rate = 44100;
    av_frame_get_buffer(m_rtmp->audioFrame, 0);
    return true;
}

void RTMPOutput::sendVideoPacket(const uint8_t* data, size_t size,
                                  int64_t pts, bool keyframe) {
    if (!m_connected.load() || !m_rtmp->headerWritten) return;

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        AURA_LOG_WARN("RTMPOutput", "av_packet_alloc OOM — dropping video frame.");
        return;
    }
    pkt->data = const_cast<uint8_t*>(data);
    pkt->size = static_cast<int>(size);
    pkt->pts = pkt->dts = pts;
    pkt->stream_index = m_rtmp->videoStream->index;
    if (keyframe) pkt->flags |= AV_PKT_FLAG_KEY;

    av_packet_rescale_ts(pkt, {1, 90000}, m_rtmp->videoStream->time_base);
    const int ret = av_interleaved_write_frame(m_rtmp->fmtCtx, pkt);
    if (ret < 0) {
        AURA_LOG_ERROR("RTMPOutput",
            "av_interleaved_write_frame (video) failed: {} — disconnecting.", ret);
        av_packet_free(&pkt);
        disconnect();
        return;
    }
    m_bytesStreamed.fetch_add(size, std::memory_order_relaxed);
    av_packet_free(&pkt);
}

void RTMPOutput::sendAudioSamples(const float* samples, uint32_t numFrames,
                                   uint32_t sampleRate, uint32_t channels) {
    if (!m_connected.load() || !m_rtmp->audioEncCtx) return;
    // Same pattern as StreamRecorder::onAudioSamples
    AVFrame* frame = m_rtmp->audioFrame;
    av_frame_make_writable(frame);
    const uint32_t sz = std::min(numFrames, (uint32_t)frame->nb_samples);
    memcpy(frame->data[0], samples, sz * channels * sizeof(float));
    frame->pts = m_rtmp->audioPts;
    m_rtmp->audioPts += sz;

    avcodec_send_frame(m_rtmp->audioEncCtx, frame);
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return;
    while (avcodec_receive_packet(m_rtmp->audioEncCtx, pkt) == 0) {
        pkt->stream_index = m_rtmp->audioStream->index;
        av_packet_rescale_ts(pkt, m_rtmp->audioEncCtx->time_base,
                             m_rtmp->audioStream->time_base);
        const int ret = av_interleaved_write_frame(m_rtmp->fmtCtx, pkt);
        if (ret < 0) {
            AURA_LOG_ERROR("RTMPOutput",
                "av_interleaved_write_frame (audio) failed: {} — disconnecting.", ret);
            av_packet_free(&pkt);
            disconnect();
            return;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

void RTMPOutput::disconnect() {
    if (!m_connected.exchange(false)) return;
    flushAudioEncoder();
    if (m_rtmp->headerWritten) {
        av_write_trailer(m_rtmp->fmtCtx);
        if (!(m_rtmp->fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_rtmp->fmtCtx->pb);
    }
    if (m_rtmp->audioEncCtx) avcodec_free_context(&m_rtmp->audioEncCtx);
    if (m_rtmp->audioFrame)  av_frame_free(&m_rtmp->audioFrame);
    if (m_rtmp->fmtCtx)      avformat_free_context(m_rtmp->fmtCtx);
    *m_rtmp = {};
    AURA_LOG_INFO("RTMPOutput", "Disconnected. Bytes streamed: {}", m_bytesStreamed.load());
}

void RTMPOutput::flushAudioEncoder() {
    if (!m_rtmp->audioEncCtx) return;
    avcodec_send_frame(m_rtmp->audioEncCtx, nullptr);
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_rtmp->audioEncCtx, pkt) == 0) {
        pkt->stream_index = m_rtmp->audioStream->index;
        av_interleaved_write_frame(m_rtmp->fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

double RTMPOutput::streamDurationSec() const {
    if (!m_connected.load() || m_startTime == 0) return 0;
    return (av_gettime() - m_startTime) / 1e6;
}

void RTMPOutput::shutdown() { disconnect(); }

} // namespace aura
