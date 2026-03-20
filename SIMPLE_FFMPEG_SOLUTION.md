# Simple FFmpeg Solution - Use What Works

## The Problem
We've been trying to manually parse AVCC format, handle NAL units, and feed Windows Media Foundation decoder. This is too complex and error-prone.

## The Solution: Use FFmpeg Like Xindawn Does

Xindawn uses **libavcodec** (part of FFmpeg) which handles:
- AVCC format parsing
- H.264 decoding
- Frame output in YUV format

We already have FFmpeg in vcpkg! We just need to use it properly.

## Implementation Plan

### Step 1: Replace VideoDecoder with FFmpegDecoder

Create a simple FFmpeg-based decoder that:
1. Takes raw TCP stream data
2. Uses `avcodec_send_packet()` to feed data
3. Uses `avcodec_receive_frame()` to get decoded frames
4. Converts YUV to RGB/NV12 for display

### Step 2: Simplify MirroringListener Callback

Instead of trying to parse AVCC:
```cpp
// Just feed raw data to FFmpeg decoder
if (g_ffmpegDecoder) {
    g_ffmpegDecoder->feedData(payload, payloadSize);
    g_ffmpegDecoder->processFrames();
}
```

### Step 3: Display Frames

FFmpeg outputs AVFrame in YUV420P format. We can:
- Convert to RGB using `sws_scale()` (FFmpeg's swscale)
- Display using existing D3D12 texture upload
- Or use SDL2 for simple display (like Xindawn does)

## Why This Will Work

1. **FFmpeg handles AVCC natively** - no manual parsing needed
2. **FFmpeg handles H.264 decoding** - no MFT complexity
3. **Proven solution** - Xindawn uses the exact same approach
4. **We already have FFmpeg** - it's in vcpkg dependencies

## Code Example (Simplified)

```cpp
class FFmpegVideoDecoder {
    AVCodecContext* codecCtx;
    AVCodec* codec;
    AVFrame* frame;
    AVPacket* packet;
    
    void init() {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        codecCtx = avcodec_alloc_context3(codec);
        avcodec_open2(codecCtx, codec, nullptr);
        frame = av_frame_alloc();
        packet = av_packet_alloc();
    }
    
    void feedData(const uint8_t* data, size_t size) {
        packet->data = (uint8_t*)data;
        packet->size = size;
        avcodec_send_packet(codecCtx, packet);
    }
    
    void processFrames() {
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            // Got a decoded frame!
            // frame->data[0] = Y plane
            // frame->data[1] = U plane  
            // frame->data[2] = V plane
            displayFrame(frame);
        }
    }
};
```

## Next Steps

1. Create `src/engine/FFmpegVideoDecoder.h/cpp`
2. Replace VideoDecoder usage with FFmpegVideoDecoder
3. Remove all AVCC parsing code from main.cpp
4. Remove NALParser (not needed)
5. Test - should work immediately

## Estimated Time: 30 minutes
vs. Current approach: Already spent 3+ hours with no success
