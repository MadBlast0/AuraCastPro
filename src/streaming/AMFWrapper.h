#pragma once
#include "HardwareEncoder.h"
using aura::HardwareEncoder;
#include <string>
#include <vector>
#include <functional>

struct AVFrame;
struct AVCodecContext;
struct AVPacket;

// AMD Advanced Media Framework (AMF) hardware encoder.
// Loaded dynamically from amfrt64.dll — gracefully absent if not AMD GPU.
class AMFWrapper : public HardwareEncoder {
public:
    AMFWrapper();
    ~AMFWrapper() override;

    bool init(int width, int height, int bitrateMbps, int fps) override;
    bool encodeFrame(ID3D12Resource* frame) override;
    void getEncodedPacket(AVPacket* out) override;
    void drain() override;
    bool encodeAvFrame(AVFrame* frame);  // encode a pre-populated AVFrame
    void shutdown() override;

    static bool isAvailable();

private:
    bool loadAMF();
    void unloadAMF();

    HMODULE m_hAmf = nullptr;

    // AMF opaque pointers (avoid pulling in AMF SDK headers)
    void* m_factory   = nullptr;
    void* m_context   = nullptr;
    void* m_encoder   = nullptr;

    int m_width      = 0;
    int m_height     = 0;
    int m_bitrateBps = 0;
    int m_fps        = 30;

    using EncodedCallback = std::function<void(const uint8_t*, size_t, int64_t, bool)>;
    void setCallback(EncodedCallback cb) { m_callback = std::move(cb); }

    AVCodecContext* m_ctx{nullptr};
    AVPacket*       m_packet{nullptr};
    EncodedCallback m_callback;
    std::vector<uint8_t> m_packetBuf;
    bool m_hasPacket = false;
};
