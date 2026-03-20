#include "../pch.h"
#include "FFmpegVideoDecoder.h"
#include "../utils/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace aura {

FFmpegVideoDecoder::FFmpegVideoDecoder() {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
    shutdown();
}

void FFmpegVideoDecoder::init() {
    AURA_LOG_INFO("FFmpegVideoDecoder", "Initializing H.264 decoder...");
    
    // Find H.264 decoder
    m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!m_codec) {
        AURA_LOG_ERROR("FFmpegVideoDecoder", "H.264 decoder not found!");
        return;
    }
    
    // Allocate codec context
    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        AURA_LOG_ERROR("FFmpegVideoDecoder", "Failed to allocate codec context");
        return;
    }
    
    // Set decoder options for low latency
    m_codecCtx->thread_count = 4;
    m_codecCtx->thread_type = FF_THREAD_FRAME;
    
    // Open codec
    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
        AURA_LOG_ERROR("FFmpegVideoDecoder", "Failed to open codec");
        avcodec_free_context(&m_codecCtx);
        return;
    }
    
    // Allocate frame and packet
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_frame || !m_packet) {
        AURA_LOG_ERROR("FFmpegVideoDecoder", "Failed to allocate frame/packet");
        shutdown();
        return;
    }
    
    m_initialized.store(true);
    AURA_LOG_INFO("FFmpegVideoDecoder", "H.264 decoder ready");
}

void FFmpegVideoDecoder::shutdown() {
    if (!m_initialized.exchange(false)) return;
    
    AURA_LOG_INFO("FFmpegVideoDecoder", "Shutting down. Decoded {} frames", m_decodedFrames.load());
    
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }
}

void FFmpegVideoDecoder::feedData(const uint8_t* data, size_t size) {
    if (!m_initialized.load() || !data || size == 0) return;
    
    // Check if this is SPS/PPS config (starts with 0x00000001 followed by NAL type 7 or 8)
    bool isSPS = (size >= 5 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1 && 
                  (data[4] & 0x1F) == 7);
    
    if (isSPS && !m_codecCtx->extradata) {
        // First time receiving SPS/PPS - set as extradata
        AURA_LOG_INFO("FFmpegVideoDecoder", "Setting SPS/PPS extradata ({} bytes)", size);
        m_codecCtx->extradata = static_cast<uint8_t*>(av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE));
        if (m_codecCtx->extradata) {
            memcpy(m_codecCtx->extradata, data, size);
            memset(m_codecCtx->extradata + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
            m_codecCtx->extradata_size = static_cast<int>(size);
            
            // Flush decoder and reopen with extradata
            avcodec_flush_buffers(m_codecCtx);
            avcodec_free_context(&m_codecCtx);
            m_codecCtx = avcodec_alloc_context3(m_codec);
            if (m_codecCtx) {
                m_codecCtx->extradata = static_cast<uint8_t*>(av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE));
                memcpy(m_codecCtx->extradata, data, size);
                memset(m_codecCtx->extradata + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
                m_codecCtx->extradata_size = static_cast<int>(size);
                m_codecCtx->thread_count = 4;
                m_codecCtx->thread_type = FF_THREAD_FRAME;
                
                if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
                    AURA_LOG_ERROR("FFmpegVideoDecoder", "Failed to reopen codec with extradata");
                } else {
                    AURA_LOG_INFO("FFmpegVideoDecoder", "Codec reopened with SPS/PPS");
                }
            }
        }
    }
    
    // Set packet data
    m_packet->data = const_cast<uint8_t*>(data);
    m_packet->size = static_cast<int>(size);
    
    // Send packet to decoder
    int ret = avcodec_send_packet(m_codecCtx, m_packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        AURA_LOG_WARN("FFmpegVideoDecoder", "avcodec_send_packet failed: {}", ret);
    }
}

void FFmpegVideoDecoder::processFrames() {
    if (!m_initialized.load()) return;
    
    while (true) {
        int ret = avcodec_receive_frame(m_codecCtx, m_frame);
        
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // Need more data
        }
        
        if (ret < 0) {
            AURA_LOG_WARN("FFmpegVideoDecoder", "avcodec_receive_frame failed: {}", ret);
            break;
        }
        
        // Got a decoded frame!
        m_decodedFrames++;
        
        if (m_decodedFrames <= 10 || m_decodedFrames % 30 == 0) {
            AURA_LOG_INFO("FFmpegVideoDecoder", "Decoded frame #{}: {}x{} format={}", 
                         m_decodedFrames.load(), m_frame->width, m_frame->height, m_frame->format);
        }
        
        if (m_frameCallback) {
            DecodedVideoFrame frame;
            frame.yPlane = m_frame->data[0];
            frame.uPlane = m_frame->data[1];
            frame.vPlane = m_frame->data[2];
            frame.width = m_frame->width;
            frame.height = m_frame->height;
            frame.yStride = m_frame->linesize[0];
            frame.uStride = m_frame->linesize[1];
            frame.vStride = m_frame->linesize[2];
            frame.pts = m_frame->pts;
            
            m_frameCallback(frame);
        }
        
        av_frame_unref(m_frame);
    }
}

void FFmpegVideoDecoder::setFrameCallback(FrameCallback cb) {
    m_frameCallback = std::move(cb);
}

} // namespace aura
