// =============================================================================
// MP4Muxer.cpp -- Fragmented MP4 via FFmpeg
// =============================================================================
#include "../../pch.h"  // PCH
#include "MP4Muxer.h"
#include "../../utils/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <filesystem>

namespace aura {

MP4Muxer::MP4Muxer() {}
MP4Muxer::~MP4Muxer() { close(); }

bool MP4Muxer::open(const std::string& path, uint32_t w, uint32_t h,
                     uint32_t fps, const std::string& codec) {
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path());

    int ret = avformat_alloc_output_context2(
        &m_fmtCtx, nullptr, "mp4", path.c_str());
    if (ret < 0 || !m_fmtCtx) return false;

    // Fragmented MP4 flags for crash-safe recording
    av_opt_set(m_fmtCtx->priv_data, "movflags",
               "frag_keyframe+empty_moov+default_base_moof", 0);

    // Video stream
    m_videoStream = avformat_new_stream(m_fmtCtx, nullptr);
    m_videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    m_videoStream->codecpar->codec_id   =
        (codec == "H265" || codec == "HEVC") ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    m_videoStream->codecpar->width  = static_cast<int>(w);
    m_videoStream->codecpar->height = static_cast<int>(h);
    m_videoStream->time_base        = {1, static_cast<int>(fps) * 1000};

    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_fmtCtx->pb, path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) { avformat_free_context(m_fmtCtx); m_fmtCtx = nullptr; return false; }
    }

    if (avformat_write_header(m_fmtCtx, nullptr) < 0) return false;

    m_open = true;
    AURA_LOG_INFO("MP4Muxer", "Opened: {} ({}x{} {} @{}fps)", path, w, h, codec, fps);
    return true;
}

void MP4Muxer::writeVideoPacket(const uint8_t* data, size_t size,
                                 int64_t pts, bool keyframe) {
    if (!m_open || !m_videoStream) return;
    AVPacket* pkt = av_packet_alloc();
    pkt->data = const_cast<uint8_t*>(data);
    pkt->size = static_cast<int>(size);
    pkt->pts = pkt->dts = pts;
    pkt->stream_index = m_videoStream->index;
    if (keyframe) pkt->flags |= AV_PKT_FLAG_KEY;
    av_packet_rescale_ts(pkt, {1, 90000}, m_videoStream->time_base);
    av_interleaved_write_frame(m_fmtCtx, pkt);
    m_bytesWritten += size;
    av_packet_free(&pkt);
}

void MP4Muxer::writeAudioPacket(const uint8_t* data, size_t size, int64_t pts) {
    if (!m_open || !m_audioStream) return;
    AVPacket* pkt = av_packet_alloc();
    pkt->data = const_cast<uint8_t*>(data);
    pkt->size = static_cast<int>(size);
    pkt->pts = pkt->dts = pts;
    pkt->stream_index = m_audioStream->index;
    av_interleaved_write_frame(m_fmtCtx, pkt);
    av_packet_free(&pkt);
}

void MP4Muxer::close() {
    if (!m_open) return;
    av_write_trailer(m_fmtCtx);
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&m_fmtCtx->pb);
    avformat_free_context(m_fmtCtx);
    m_fmtCtx = nullptr;
    m_videoStream = m_audioStream = nullptr;
    m_open = false;
    AURA_LOG_INFO("MP4Muxer", "Closed. Bytes written: {}", m_bytesWritten);
}

} // namespace aura
