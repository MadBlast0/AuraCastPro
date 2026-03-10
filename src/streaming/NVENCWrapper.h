#pragma once
// =============================================================================
// NVENCWrapper.h — Direct NVENC SDK wrapper (bypasses FFmpeg for lowest latency)
//
// Direct NVENC SDK gives ~2ms less encode latency vs going through FFmpeg.
// Falls back to HardwareEncoder (FFmpeg h264_nvenc) if NVENC SDK not present.
// =============================================================================
#include <memory>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>

struct ID3D12Resource;
struct ID3D12Device;

namespace aura {

class NVENCWrapper {
public:
    using PacketCallback = std::function<void(const uint8_t*, size_t, int64_t, bool)>;

    NVENCWrapper();
    ~NVENCWrapper();

    void init();
    void shutdown();

    bool open(ID3D12Device* device, uint32_t w, uint32_t h,
              uint32_t fps, int bitrateKbps, const char* codec = "H264");
    void close();

    void encodeFrame(ID3D12Resource* nv12Texture, int64_t pts);
    void setCallback(PacketCallback cb);

    bool isAvailable() const;
    bool isOpen()      const { return m_open.load(); }

private:
    std::atomic<bool> m_open{false};
    PacketCallback    m_callback;

    struct NVState;
    std::unique_ptr<NVState> m_nv;
};

} // namespace aura
