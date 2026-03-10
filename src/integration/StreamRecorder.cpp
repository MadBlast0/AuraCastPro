// =============================================================================
// StreamRecorder.cpp — Fragmented MP4 recording via FFmpeg libavformat
//
// Uses fragmented MP4 (frag_keyframe+empty_moov) so the file is playable
// even if the app crashes mid-recording.
//
// Video: stream copy (no re-encode) — H.265 in → H.265 out. Zero CPU cost.
// Audio: PCM float 48kHz → AAC-LC 192kbps (FFmpeg AAC encoder).
//
// FFmpeg API flow:
//   avformat_alloc_output_context2 ("mp4")
//   → add video stream (copy codec params from input)
//   → add audio stream (AAC encoder)
//   → av_opt_set movflags "frag_keyframe+empty_moov+default_base_moof"
//   → avio_open / avformat_write_header
//   → av_interleaved_write_frame (per packet/frame)
//   → av_write_trailer / avio_closep
// =============================================================================

#include "StreamRecorder.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
}

#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <format>

namespace aura {

struct StreamRecorder::FFmpegState {
    AVFormatContext* fmtCtx{nullptr};
    AVStream*        videoStream{nullptr};
    AVStream*        audioStream{nullptr};

    // Audio encoder
    AVCodecContext*  audioEncCtx{nullptr};
    SwrContext*      swrCtx{nullptr};    // PCM float → AACencoder format
    AVFrame*         audioFrame{nullptr};
    int64_t          audioPts{0};

    int64_t  startTimestamp{0};
    bool     headerWritten{false};
};

// -----------------------------------------------------------------------------
StreamRecorder::StreamRecorder()
    : m_ffmpeg(std::make_unique<FFmpegState>()) {}

StreamRecorder::~StreamRecorder() { shutdown(); }

void StreamRecorder::init() {
    AURA_LOG_INFO("StreamRecorder",
        "Initialised. FFmpeg fragmented MP4. "
        "Video: stream copy (zero CPU). Audio: PCM→AAC-LC 192kbps.");
}

// -----------------------------------------------------------------------------
bool StreamRecorder::startRecording(const std::string& outputPath,
                                     uint32_t videoWidth, uint32_t videoHeight,
                                     uint32_t fps, const std::string& videoCodec) {
    if (m_recording.load()) stopRecording();

    // ── Pre-start disk space check (Task 135/137 fix) ─────────────────────────
    // Block start if < 1 GB free on the recording drive
    static constexpr uint64_t kMinStartBytes = 1ULL * 1024 * 1024 * 1024; // 1 GB
    try {
        const auto space = std::filesystem::space(
            std::filesystem::path(outputPath).parent_path());
        if (space.available < kMinStartBytes) {
            AURA_LOG_ERROR("StreamRecorder",
                "BLOCKED: Not enough disk space. Need >= 1 GB, have {:.0f} MB free.",
                space.available / (1024.0 * 1024.0));
            return false;
        }
    } catch (const std::exception& e) {
        AURA_LOG_WARN("StreamRecorder", "Disk space check failed: {}", e.what());
        // Non-fatal — continue recording attempt
    }

    AURA_LOG_INFO("StreamRecorder", "Starting recording: {}", outputPath);

    // Ensure output directory exists
    std::filesystem::create_directories(
        std::filesystem::path(outputPath).parent_path());

    // 1. Allocate output context
    int ret = avformat_alloc_output_context2(
        &m_ffmpeg->fmtCtx, nullptr, "mp4", outputPath.c_str());
    if (ret < 0 || !m_ffmpeg->fmtCtx) {
        AURA_LOG_ERROR("StreamRecorder",
            "avformat_alloc_output_context2 failed: {}", ret);
        return false;
    }

    // 2. Set fragmented MP4 flags (crash-safe recording)
    av_opt_set(m_ffmpeg->fmtCtx->priv_data, "movflags",
               "frag_keyframe+empty_moov+default_base_moof", 0);

    // 3. Add video stream (stream copy)
    if (!initVideoStream(videoWidth, videoHeight, fps, videoCodec)) return false;

    // 4. Add audio stream (AAC encoder)
    if (!initAudioStream(48000, 2)) return false;

    // 5. Open output file
    if (!(m_ffmpeg->fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_ffmpeg->fmtCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            AURA_LOG_ERROR("StreamRecorder", "avio_open failed for {}: {}", outputPath, ret);
            return false;
        }
    }

    // 6. Write container header
    ret = avformat_write_header(m_ffmpeg->fmtCtx, nullptr);
    if (ret < 0) {
        AURA_LOG_ERROR("StreamRecorder", "avformat_write_header failed: {}", ret);
        return false;
    }

    m_ffmpeg->headerWritten = true;
    m_ffmpeg->startTimestamp = av_gettime();
    m_startPts = 0;
    m_bytesWritten.store(0);
    m_recording.store(true);
    m_paused.store(false);

    AURA_LOG_INFO("StreamRecorder", "Recording started: {}×{} {} @{}fps → {}",
                  videoWidth, videoHeight, videoCodec, fps, outputPath);
    return true;
}

// -----------------------------------------------------------------------------
bool StreamRecorder::initVideoStream(uint32_t w, uint32_t h, uint32_t fps,
                                      const std::string& codec) {
    m_ffmpeg->videoStream = avformat_new_stream(m_ffmpeg->fmtCtx, nullptr);
    if (!m_ffmpeg->videoStream) return false;

    // Stream copy: set codec parameters directly
    AVCodecParameters* par = m_ffmpeg->videoStream->codecpar;
    par->codec_type  = AVMEDIA_TYPE_VIDEO;
    par->codec_id    = (codec == "H265" || codec == "HEVC")
                       ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    par->width       = static_cast<int>(w);
    par->height      = static_cast<int>(h);
    par->format      = AV_PIX_FMT_YUV420P;

    m_ffmpeg->videoStream->time_base = {1, static_cast<int>(fps) * 1000};
    m_ffmpeg->videoStream->avg_frame_rate = {static_cast<int>(fps), 1};
    return true;
}

// -----------------------------------------------------------------------------
bool StreamRecorder::initAudioStream(uint32_t sampleRate, uint32_t channels) {
    const AVCodec* aacEncoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!aacEncoder) {
        AURA_LOG_WARN("StreamRecorder", "AAC encoder not found. Audio will not be recorded.");
        return true; // Non-fatal — record video only
    }

    m_ffmpeg->audioEncCtx = avcodec_alloc_context3(aacEncoder);
    if (!m_ffmpeg->audioEncCtx) return false;

    m_ffmpeg->audioEncCtx->sample_rate    = static_cast<int>(sampleRate);
    m_ffmpeg->audioEncCtx->ch_layout      = AV_CHANNEL_LAYOUT_STEREO;
    m_ffmpeg->audioEncCtx->sample_fmt     = aacEncoder->sample_fmts[0];
    m_ffmpeg->audioEncCtx->bit_rate       = 192000; // 192 kbps
    m_ffmpeg->audioEncCtx->time_base      = {1, static_cast<int>(sampleRate)};

    if (m_ffmpeg->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_ffmpeg->audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_ffmpeg->audioEncCtx, aacEncoder, nullptr) < 0) {
        AURA_LOG_WARN("StreamRecorder", "Failed to open AAC encoder.");
        return true; // Non-fatal
    }

    m_ffmpeg->audioStream = avformat_new_stream(m_ffmpeg->fmtCtx, nullptr);
    avcodec_parameters_from_context(m_ffmpeg->audioStream->codecpar,
                                    m_ffmpeg->audioEncCtx);
    m_ffmpeg->audioStream->time_base = m_ffmpeg->audioEncCtx->time_base;

    // Audio frame for encoding
    m_ffmpeg->audioFrame = av_frame_alloc();
    m_ffmpeg->audioFrame->nb_samples     = m_ffmpeg->audioEncCtx->frame_size;
    m_ffmpeg->audioFrame->format         = m_ffmpeg->audioEncCtx->sample_fmt;
    m_ffmpeg->audioFrame->ch_layout      = AV_CHANNEL_LAYOUT_STEREO;
    m_ffmpeg->audioFrame->sample_rate    = static_cast<int>(sampleRate);
    av_frame_get_buffer(m_ffmpeg->audioFrame, 0);

    return true;
}

// -----------------------------------------------------------------------------
void StreamRecorder::onVideoPacket(const uint8_t* data, size_t size,
                                    int64_t pts, bool isKeyframe) {
    if (!m_recording.load() || m_paused.load() || !m_ffmpeg->headerWritten) return;
    if (!m_ffmpeg->videoStream) return;

    AVPacket* pkt = av_packet_alloc();
    pkt->data = const_cast<uint8_t*>(data);
    pkt->size = static_cast<int>(size);
    pkt->pts  = pkt->dts = pts;
    pkt->stream_index = m_ffmpeg->videoStream->index;
    if (isKeyframe) pkt->flags |= AV_PKT_FLAG_KEY;

    av_packet_rescale_ts(pkt,
        {1, 90000},  // RTP timestamp base
        m_ffmpeg->videoStream->time_base);

    const int ret = av_interleaved_write_frame(m_ffmpeg->fmtCtx, pkt);
    if (ret < 0) AURA_LOG_WARN("StreamRecorder", "Video write error: {}", ret);

    m_bytesWritten.fetch_add(size, std::memory_order_relaxed);
    av_packet_free(&pkt);
}

// -----------------------------------------------------------------------------
void StreamRecorder::onAudioSamples(const float* samples, uint32_t numFrames,
                                     uint32_t sampleRate, uint32_t channels) {
    if (!m_recording.load() || m_paused.load()) return;
    if (!m_ffmpeg->audioEncCtx || !m_ffmpeg->audioFrame) return;

    // Copy samples into AVFrame and encode
    // (simplified — full implementation uses SwrContext for format conversion)
    AVFrame* frame = m_ffmpeg->audioFrame;
    av_frame_make_writable(frame);

    const uint32_t frameSz = std::min(numFrames,
        static_cast<uint32_t>(frame->nb_samples));
    memcpy(frame->data[0], samples,
           frameSz * channels * sizeof(float));

    frame->pts = m_ffmpeg->audioPts;
    m_ffmpeg->audioPts += frameSz;

    // Send frame to encoder
    avcodec_send_frame(m_ffmpeg->audioEncCtx, frame);

    // Receive encoded packets
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_ffmpeg->audioEncCtx, pkt) == 0) {
        pkt->stream_index = m_ffmpeg->audioStream->index;
        av_packet_rescale_ts(pkt,
            m_ffmpeg->audioEncCtx->time_base,
            m_ffmpeg->audioStream->time_base);
        av_interleaved_write_frame(m_ffmpeg->fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

// -----------------------------------------------------------------------------
void StreamRecorder::stopRecording() {
    if (!m_recording.exchange(false)) return;

    if (m_ffmpeg->headerWritten) {
        flushAudioEncoder();
        av_write_trailer(m_ffmpeg->fmtCtx); // CRITICAL: writes moov atom

        if (!(m_ffmpeg->fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_ffmpeg->fmtCtx->pb);
    }

    if (m_ffmpeg->audioEncCtx) {
        avcodec_free_context(&m_ffmpeg->audioEncCtx);
        m_ffmpeg->audioEncCtx = nullptr;
    }
    if (m_ffmpeg->audioFrame) {
        av_frame_free(&m_ffmpeg->audioFrame);
        m_ffmpeg->audioFrame = nullptr;
    }
    if (m_ffmpeg->fmtCtx) {
        avformat_free_context(m_ffmpeg->fmtCtx);
        m_ffmpeg->fmtCtx = nullptr;
    }
    m_ffmpeg->headerWritten = false;
    m_ffmpeg->videoStream = nullptr;
    m_ffmpeg->audioStream = nullptr;

    AURA_LOG_INFO("StreamRecorder",
        "Recording stopped. Bytes written: {}", m_bytesWritten.load());
}

void StreamRecorder::pauseRecording()  { m_paused.store(true); }
void StreamRecorder::resumeRecording() { m_paused.store(false); }

void StreamRecorder::flushAudioEncoder() {
    if (!m_ffmpeg->audioEncCtx) return;
    avcodec_send_frame(m_ffmpeg->audioEncCtx, nullptr); // flush
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_ffmpeg->audioEncCtx, pkt) == 0) {
        pkt->stream_index = m_ffmpeg->audioStream->index;
        av_interleaved_write_frame(m_ffmpeg->fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

double StreamRecorder::durationSec() const {
    if (!m_recording.load() || m_ffmpeg->startTimestamp == 0) return 0;
    return (av_gettime() - m_ffmpeg->startTimestamp) / 1e6;
}

std::string StreamRecorder::generateOutputPath(const std::string& folder) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream ss;
    ss << folder << "\\AuraCastPro_"
       << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return ss.str();
}

void StreamRecorder::shutdown() { stopRecording(); }

} // namespace aura
