// =============================================================================
// StreamRecorder.cpp -- Fragmented MP4 recording via FFmpeg libavformat
//
// Uses fragmented MP4 (frag_keyframe+empty_moov) so the file is playable
// even if the app crashes mid-recording.
//
// Video: stream copy (no re-encode) -- H.265 in -> H.265 out. Zero CPU cost.
// Audio: PCM float 48kHz -> AAC-LC 192kbps (FFmpeg AAC encoder).
//
// FFmpeg API flow:
//   avformat_alloc_output_context2 ("mp4")
//   -> add video stream (copy codec params from input)
//   -> add audio stream (AAC encoder)
//   -> av_opt_set movflags "frag_keyframe+empty_moov+default_base_moof"
//   -> avio_open / avformat_write_header
//   -> av_interleaved_write_frame (per packet/frame)
//   -> av_write_trailer / avio_closep
// =============================================================================

#include "../pch.h"  // PCH
#include "StreamRecorder.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
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

    // Video encoder (if re-encoding)
    AVCodecContext*  videoEncCtx{nullptr};
    SwsContext*      swsCtx{nullptr};     // For rescaling
    AVFrame*         scaledFrame{nullptr}; // Rescaled frame buffer
    
    // Audio encoder
    AVCodecContext*  audioEncCtx{nullptr};
    SwrContext*      swrCtx{nullptr};    // PCM float -> AACencoder format
    AVFrame*         audioFrame{nullptr};
    int64_t          audioPts{0};

    int64_t  startTimestamp{0};
    bool     headerWritten{false};
    uint64_t currentFileSizeBytes{0};
    int      filePartNumber{0};
    std::string baseOutputPath;
    std::chrono::steady_clock::time_point fileStartTime;
};

// -----------------------------------------------------------------------------
StreamRecorder::StreamRecorder()
    : m_ffmpeg(std::make_unique<FFmpegState>()) {}

StreamRecorder::~StreamRecorder() { shutdown(); }

void StreamRecorder::init() {
    AURA_LOG_INFO("StreamRecorder",
        "Initialised. FFmpeg fragmented MP4. "
        "Video: stream copy (zero CPU). Audio: PCM->AAC-LC 192kbps.");
}

// -----------------------------------------------------------------------------
bool StreamRecorder::startRecording(const std::string& outputPath,
                                     uint32_t videoWidth, uint32_t videoHeight,
                                     uint32_t fps, const std::string& videoCodec) {
    // Legacy overload - convert to new config format
    RecordingConfig config;
    config.videoWidth = videoWidth;
    config.videoHeight = videoHeight;
    config.fps = fps;
    config.videoCodec = videoCodec;
    config.containerFormat = "mp4";
    config.videoEncoder = "copy";
    config.audioEncoder = "aac";
    return startRecording(outputPath, config);
}

// -----------------------------------------------------------------------------
bool StreamRecorder::startRecording(const std::string& outputPath, const RecordingConfig& config) {
    if (m_recording.load()) stopRecording();
    
    m_config = config;  // Store config for later use

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
        // Non-fatal -- continue recording attempt
    }

    AURA_LOG_INFO("StreamRecorder", "Starting recording: {}", outputPath);

    // Ensure output directory exists
    std::filesystem::create_directories(
        std::filesystem::path(outputPath).parent_path());

    // Map container format string to FFmpeg format name
    std::string ffmpegFormat = "mp4";
    if (config.containerFormat.find("mkv") != std::string::npos) {
        ffmpegFormat = "matroska";
    } else if (config.containerFormat.find("flv") != std::string::npos) {
        ffmpegFormat = "flv";
    } else if (config.containerFormat.find("mov") != std::string::npos) {
        ffmpegFormat = "mov";
    } else if (config.containerFormat.find("avi") != std::string::npos) {
        ffmpegFormat = "avi";
    }

    // 1. Allocate output context
    int ret = avformat_alloc_output_context2(
        &m_ffmpeg->fmtCtx, nullptr, ffmpegFormat.c_str(), outputPath.c_str());
    if (ret < 0 || !m_ffmpeg->fmtCtx) {
        AURA_LOG_ERROR("StreamRecorder",
            "avformat_alloc_output_context2 failed: {}", ret);
        return false;
    }

    // 2. Set fragmented MP4 flags (crash-safe recording) - only for MP4/MOV
    if (ffmpegFormat == "mp4" || ffmpegFormat == "mov") {
        av_opt_set(m_ffmpeg->fmtCtx->priv_data, "movflags",
                   "frag_keyframe+empty_moov+default_base_moof", 0);
    }

    // 3. Add video stream (stream copy or encode)
    if (!initVideoStream()) return false;

    // 3b. Initialize rescaler if needed
    if (!initRescaler()) return false;

    // 4. Add audio stream (AAC encoder or other)
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
    m_ffmpeg->baseOutputPath = outputPath;
    m_ffmpeg->fileStartTime = std::chrono::steady_clock::now();
    m_startPts = 0;
    m_bytesWritten.store(0);
    m_recording.store(true);
    m_paused.store(false);

    // Initialize rescaler if needed
    if (!m_config.rescaleResolution.empty() && m_config.rescaleResolution != "Source") {
        initRescaler();
    }

    AURA_LOG_INFO("StreamRecorder", "Recording started: {}x{} {} @{}fps format={} -> {}",
                  config.videoWidth, config.videoHeight, config.videoCodec, config.fps, 
                  ffmpegFormat, outputPath);
    return true;
}

// -----------------------------------------------------------------------------
bool StreamRecorder::initVideoStream() {
    m_ffmpeg->videoStream = avformat_new_stream(m_ffmpeg->fmtCtx, nullptr);
    if (!m_ffmpeg->videoStream) return false;

    // Check if we're doing stream copy or re-encoding
    const bool isStreamCopy = (m_config.videoEncoder == "copy" || 
                               m_config.videoEncoder.find("Stream Copy") != std::string::npos);

    if (isStreamCopy) {
        // Stream copy: set codec parameters directly (no re-encoding)
        AVCodecParameters* par = m_ffmpeg->videoStream->codecpar;
        par->codec_type  = AVMEDIA_TYPE_VIDEO;
        par->codec_id    = (m_config.videoCodec == "h265" || m_config.videoCodec == "hevc" || m_config.videoCodec == "H265" || m_config.videoCodec == "HEVC")
                           ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
        par->width       = static_cast<int>(m_config.videoWidth);
        par->height      = static_cast<int>(m_config.videoHeight);
        par->format      = AV_PIX_FMT_YUV420P;
        par->bit_rate    = m_config.videoBitrateKbps * 1000;

        m_ffmpeg->videoStream->time_base = {1, static_cast<int>(m_config.fps) * 1000};
        m_ffmpeg->videoStream->avg_frame_rate = {static_cast<int>(m_config.fps), 1};
        
        AURA_LOG_INFO("StreamRecorder", "Video stream: copy mode (no re-encoding)");
    } else {
        // Re-encoding mode: set up encoder
        std::string encoderName = mapEncoderName(m_config.videoEncoder);
        const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName.c_str());
        if (!encoder) {
            AURA_LOG_ERROR("StreamRecorder", "Video encoder '{}' not found", encoderName);
            return false;
        }

        m_ffmpeg->videoEncCtx = avcodec_alloc_context3(encoder);
        if (!m_ffmpeg->videoEncCtx) return false;

        // Basic settings
        m_ffmpeg->videoEncCtx->width = m_config.videoWidth;
        m_ffmpeg->videoEncCtx->height = m_config.videoHeight;
        m_ffmpeg->videoEncCtx->time_base = {1, static_cast<int>(m_config.fps)};
        m_ffmpeg->videoEncCtx->framerate = {static_cast<int>(m_config.fps), 1};
        m_ffmpeg->videoEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        m_ffmpeg->videoEncCtx->bit_rate = m_config.videoBitrateKbps * 1000;

        // Apply all encoder settings
        applyEncoderSettings(m_ffmpeg->videoEncCtx);

        if (m_ffmpeg->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
            m_ffmpeg->videoEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(m_ffmpeg->videoEncCtx, encoder, nullptr) < 0) {
            AURA_LOG_ERROR("StreamRecorder", "Failed to open video encoder");
            return false;
        }

        avcodec_parameters_from_context(m_ffmpeg->videoStream->codecpar, m_ffmpeg->videoEncCtx);
        m_ffmpeg->videoStream->time_base = m_ffmpeg->videoEncCtx->time_base;
        
        AURA_LOG_INFO("StreamRecorder", "Video stream: encoding with {}", encoderName);
    }

    return true;
}

// -----------------------------------------------------------------------------
std::string StreamRecorder::mapEncoderName(const std::string& encoderSetting) {
    // Map UI encoder names to FFmpeg encoder names
    if (encoderSetting.find("NVENC H.264") != std::string::npos) return "h264_nvenc";
    if (encoderSetting.find("NVENC H.265") != std::string::npos) return "hevc_nvenc";
    if (encoderSetting.find("x264") != std::string::npos) return "libx264";
    if (encoderSetting.find("x265") != std::string::npos) return "libx265";
    if (encoderSetting.find("AMD AMF H.264") != std::string::npos) return "h264_amf";
    if (encoderSetting.find("Intel QSV H.264") != std::string::npos) return "h264_qsv";
    return "libx264"; // Default fallback
}

// -----------------------------------------------------------------------------
void StreamRecorder::applyEncoderSettings(AVCodecContext* ctx) {
    if (!ctx) return;

    // Rate control
    if (m_config.rateControl.find("Constant Bitrate") != std::string::npos || 
        m_config.rateControl == "cbr") {
        av_opt_set(ctx->priv_data, "rc", "cbr", 0);
    } else if (m_config.rateControl.find("Variable Bitrate") != std::string::npos || 
               m_config.rateControl == "vbr") {
        av_opt_set(ctx->priv_data, "rc", "vbr", 0);
    } else if (m_config.rateControl.find("Constant QP") != std::string::npos || 
               m_config.rateControl == "cqp") {
        av_opt_set(ctx->priv_data, "rc", "cqp", 0);
    } else if (m_config.rateControl.find("Lossless") != std::string::npos) {
        av_opt_set(ctx->priv_data, "preset", "lossless", 0);
    }

    // Encoder preset (map UI names to FFmpeg presets)
    std::string preset = "medium";
    if (m_config.encoderPreset.find("P1") != std::string::npos || 
        m_config.encoderPreset.find("Fastest") != std::string::npos) {
        preset = "ultrafast";
    } else if (m_config.encoderPreset.find("P2") != std::string::npos || 
               m_config.encoderPreset.find("Faster") != std::string::npos) {
        preset = "superfast";
    } else if (m_config.encoderPreset.find("P3") != std::string::npos || 
               m_config.encoderPreset.find("Fast") != std::string::npos) {
        preset = "fast";
    } else if (m_config.encoderPreset.find("P4") != std::string::npos || 
               m_config.encoderPreset.find("Medium") != std::string::npos) {
        preset = "medium";
    } else if (m_config.encoderPreset.find("P5") != std::string::npos || 
               m_config.encoderPreset.find("Slow") != std::string::npos) {
        preset = "slow";
    } else if (m_config.encoderPreset.find("P6") != std::string::npos || 
               m_config.encoderPreset.find("Slower") != std::string::npos) {
        preset = "slower";
    } else if (m_config.encoderPreset.find("P7") != std::string::npos || 
               m_config.encoderPreset.find("Slowest") != std::string::npos) {
        preset = "veryslow";
    }
    av_opt_set(ctx->priv_data, "preset", preset.c_str(), 0);

    // Profile
    if (!m_config.encoderProfile.empty()) {
        av_opt_set(ctx->priv_data, "profile", m_config.encoderProfile.c_str(), 0);
    }

    // Tuning
    if (m_config.encoderTuning.find("High Quality") != std::string::npos) {
        av_opt_set(ctx->priv_data, "tune", "hq", 0);
    } else if (m_config.encoderTuning.find("Low Latency") != std::string::npos) {
        av_opt_set(ctx->priv_data, "tune", "ll", 0);
    } else if (m_config.encoderTuning.find("Ultra Low Latency") != std::string::npos) {
        av_opt_set(ctx->priv_data, "tune", "ull", 0);
    } else if (m_config.encoderTuning.find("Lossless") != std::string::npos) {
        av_opt_set(ctx->priv_data, "tune", "lossless", 0);
    }

    // Keyframe interval
    if (m_config.keyframeInterval > 0) {
        ctx->gop_size = m_config.keyframeInterval;
    } else {
        ctx->gop_size = m_config.fps * 2; // Default: 2 seconds
    }

    // B-frames
    ctx->max_b_frames = m_config.bFrames;

    // Look ahead
    if (m_config.lookAhead) {
        av_opt_set(ctx->priv_data, "rc-lookahead", "32", 0);
    }

    // Adaptive quantization
    if (m_config.adaptiveQuantization) {
        av_opt_set(ctx->priv_data, "aq-mode", "2", 0);
    }

    // Multipass
    if (m_config.multipassMode.find("Two") != std::string::npos) {
        av_opt_set(ctx->priv_data, "multipass", "fullres", 0);
    }

    // Custom encoder options
    if (!m_config.customEncoderOptions.empty()) {
        // Parse custom options (format: "key1=value1:key2=value2")
        std::string opts = m_config.customEncoderOptions;
        size_t pos = 0;
        while ((pos = opts.find(':')) != std::string::npos) {
            std::string pair = opts.substr(0, pos);
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                std::string key = pair.substr(0, eq);
                std::string val = pair.substr(eq + 1);
                av_opt_set(ctx->priv_data, key.c_str(), val.c_str(), 0);
            }
            opts.erase(0, pos + 1);
        }
    }

    AURA_LOG_INFO("StreamRecorder", "Applied encoder settings: preset={}, profile={}, gop={}, bframes={}", 
                  preset, m_config.encoderProfile, ctx->gop_size, ctx->max_b_frames);
}

// -----------------------------------------------------------------------------
bool StreamRecorder::initRescaler() {
    if (m_config.rescaleResolution.empty() || m_config.rescaleResolution == "Source") {
        return true; // No rescaling needed
    }

    // Parse target resolution (format: "1920x1080")
    int targetWidth = m_config.videoWidth;
    int targetHeight = m_config.videoHeight;
    
    size_t xPos = m_config.rescaleResolution.find('x');
    if (xPos != std::string::npos) {
        targetWidth = std::stoi(m_config.rescaleResolution.substr(0, xPos));
        targetHeight = std::stoi(m_config.rescaleResolution.substr(xPos + 1));
    }

    // Map rescale filter to SwScale flags
    int swsFlags = SWS_LANCZOS; // Default
    if (m_config.rescaleFilter.find("Bicubic") != std::string::npos) {
        swsFlags = SWS_BICUBIC;
    } else if (m_config.rescaleFilter.find("Bilinear") != std::string::npos) {
        swsFlags = SWS_BILINEAR;
    } else if (m_config.rescaleFilter.find("Nearest") != std::string::npos) {
        swsFlags = SWS_POINT;
    }

    m_ffmpeg->swsCtx = sws_getContext(
        m_config.videoWidth, m_config.videoHeight, AV_PIX_FMT_YUV420P,
        targetWidth, targetHeight, AV_PIX_FMT_YUV420P,
        swsFlags, nullptr, nullptr, nullptr
    );

    if (!m_ffmpeg->swsCtx) {
        AURA_LOG_ERROR("StreamRecorder", "Failed to create SwsContext for rescaling");
        return false;
    }

    // Allocate scaled frame buffer
    m_ffmpeg->scaledFrame = av_frame_alloc();
    m_ffmpeg->scaledFrame->width = targetWidth;
    m_ffmpeg->scaledFrame->height = targetHeight;
    m_ffmpeg->scaledFrame->format = AV_PIX_FMT_YUV420P;
    av_frame_get_buffer(m_ffmpeg->scaledFrame, 0);

    AURA_LOG_INFO("StreamRecorder", "Rescaler initialized: {}x{} -> {}x{} using {}", 
                  m_config.videoWidth, m_config.videoHeight, 
                  targetWidth, targetHeight, m_config.rescaleFilter);
    return true;
}

// -----------------------------------------------------------------------------
bool StreamRecorder::shouldSplitFile() {
    if (!m_config.autoFileSplitting) return false;

    if (m_config.fileSplitMode.find("Size") != std::string::npos) {
        // Split by file size
        uint64_t maxBytes = m_config.fileSplitSizeMB * 1024ULL * 1024ULL;
        return m_ffmpeg->currentFileSizeBytes >= maxBytes;
    } else {
        // Split by time
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - m_ffmpeg->fileStartTime).count();
        return elapsed >= m_config.fileSplitTimeMinutes;
    }
}

// -----------------------------------------------------------------------------
std::string StreamRecorder::generateSplitFilePath() {
    // Extract base path and extension
    std::string base = m_ffmpeg->baseOutputPath;
    size_t extPos = base.find_last_of('.');
    std::string baseName = base.substr(0, extPos);
    std::string extension = base.substr(extPos);
    
    // Generate part filename
    m_ffmpeg->filePartNumber++;
    std::ostringstream ss;
    ss << baseName << "_part" << std::setfill('0') << std::setw(3) 
       << m_ffmpeg->filePartNumber << extension;
    return ss.str();
}

// -----------------------------------------------------------------------------
bool StreamRecorder::splitToNewFile() {
    if (!m_recording.load()) return false;

    AURA_LOG_INFO("StreamRecorder", "Splitting to new file (part {})", m_ffmpeg->filePartNumber + 1);

    // Close current file
    if (m_ffmpeg->headerWritten) {
        av_write_trailer(m_ffmpeg->fmtCtx);
        if (!(m_ffmpeg->fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_ffmpeg->fmtCtx->pb);
    }

    // Generate new file path
    std::string newPath = generateSplitFilePath();

    // Open new file
    int ret = avio_open(&m_ffmpeg->fmtCtx->pb, newPath.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        AURA_LOG_ERROR("StreamRecorder", "Failed to open split file: {}", newPath);
        return false;
    }

    // Write new header
    ret = avformat_write_header(m_ffmpeg->fmtCtx, nullptr);
    if (ret < 0) {
        AURA_LOG_ERROR("StreamRecorder", "Failed to write header for split file");
        return false;
    }

    m_ffmpeg->currentFileSizeBytes = 0;
    m_ffmpeg->fileStartTime = std::chrono::steady_clock::now();

    AURA_LOG_INFO("StreamRecorder", "Split to new file: {}", newPath);
    return true;
}

// -----------------------------------------------------------------------------
AVCodecID StreamRecorder::mapAudioCodecId(const std::string& audioEncoder) {
    if (audioEncoder.find("AAC") != std::string::npos || audioEncoder == "aac") 
        return AV_CODEC_ID_AAC;
    if (audioEncoder.find("Opus") != std::string::npos || audioEncoder == "opus") 
        return AV_CODEC_ID_OPUS;
    if (audioEncoder.find("MP3") != std::string::npos || audioEncoder == "mp3") 
        return AV_CODEC_ID_MP3;
    if (audioEncoder.find("FLAC") != std::string::npos || audioEncoder == "flac") 
        return AV_CODEC_ID_FLAC;
    return AV_CODEC_ID_AAC; // Default
}

// -----------------------------------------------------------------------------
bool StreamRecorder::initAudioStream(uint32_t sampleRate, uint32_t channels) {
    // Check if audio track is enabled (bit 0 of audioTrackMask)
    if ((m_config.audioTrackMask & 1) == 0) {
        AURA_LOG_INFO("StreamRecorder", "Audio track disabled by settings");
        return true; // Not an error, just skip audio
    }

    AVCodecID codecId = mapAudioCodecId(m_config.audioEncoder);
    const AVCodec* audioEncoder = avcodec_find_encoder(codecId);
    if (!audioEncoder) {
        AURA_LOG_WARN("StreamRecorder", "Audio encoder not found. Audio will not be recorded.");
        return true; // Non-fatal -- record video only
    }

    m_ffmpeg->audioEncCtx = avcodec_alloc_context3(audioEncoder);
    if (!m_ffmpeg->audioEncCtx) return false;

    m_ffmpeg->audioEncCtx->sample_rate    = static_cast<int>(sampleRate);
    m_ffmpeg->audioEncCtx->ch_layout      = AV_CHANNEL_LAYOUT_STEREO;
    m_ffmpeg->audioEncCtx->sample_fmt     = audioEncoder->sample_fmts[0];
    m_ffmpeg->audioEncCtx->bit_rate       = m_config.audioBitrateKbps * 1000;
    m_ffmpeg->audioEncCtx->time_base      = {1, static_cast<int>(sampleRate)};

    if (m_ffmpeg->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_ffmpeg->audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_ffmpeg->audioEncCtx, audioEncoder, nullptr) < 0) {
        AURA_LOG_WARN("StreamRecorder", "Failed to open audio encoder.");
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

    AURA_LOG_INFO("StreamRecorder", "Audio stream: {} @ {}kbps", 
                  m_config.audioEncoder, m_config.audioBitrateKbps);
    return true;
}

// -----------------------------------------------------------------------------
void StreamRecorder::onVideoPacket(const uint8_t* data, size_t size,
                                    int64_t pts, bool isKeyframe) {
    if (!m_recording.load() || m_paused.load() || !m_ffmpeg->headerWritten) return;
    if (!m_ffmpeg->videoStream) return;

    // Check if we need to split to a new file (only on keyframes)
    if (isKeyframe && shouldSplitFile()) {
        if (!splitToNewFile()) {
            AURA_LOG_ERROR("StreamRecorder", "File split failed, continuing with current file");
        }
    }

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

    const size_t bytesWritten = size;
    m_bytesWritten.fetch_add(bytesWritten, std::memory_order_relaxed);
    m_ffmpeg->currentFileSizeBytes += bytesWritten;
    
    av_packet_free(&pkt);
}

// -----------------------------------------------------------------------------
void StreamRecorder::onAudioSamples(const float* samples, uint32_t numFrames,
                                     uint32_t sampleRate, uint32_t channels) {
    if (!m_recording.load() || m_paused.load()) return;
    if (!m_ffmpeg->audioEncCtx || !m_ffmpeg->audioFrame) return;

    // Copy samples into AVFrame and encode
    // (simplified -- full implementation uses SwrContext for format conversion)
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

    if (m_ffmpeg->videoEncCtx) {
        avcodec_free_context(&m_ffmpeg->videoEncCtx);
        m_ffmpeg->videoEncCtx = nullptr;
    }
    if (m_ffmpeg->swsCtx) {
        sws_freeContext(m_ffmpeg->swsCtx);
        m_ffmpeg->swsCtx = nullptr;
    }
    if (m_ffmpeg->scaledFrame) {
        av_frame_free(&m_ffmpeg->scaledFrame);
        m_ffmpeg->scaledFrame = nullptr;
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
    m_ffmpeg->currentFileSizeBytes = 0;
    m_ffmpeg->filePartNumber = 0;

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

std::string StreamRecorder::generateOutputPath(const std::string& folder,
                                                const std::string& extension,
                                                bool noSpaces) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream ss;
    ss << folder << "\\AuraCastPro";
    if (noSpaces) {
        ss << "_" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    } else {
        ss << " " << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
    }
    ss << extension;
    return ss.str();
}

void StreamRecorder::shutdown() { stopRecording(); }

} // namespace aura
