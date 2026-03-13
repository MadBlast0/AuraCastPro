#pragma once
// =============================================================================
// VCamBridge.h -- Virtual camera output (phone screen -> OBS/Zoom camera source)
//
// Copies rendered frames from the DX12 render target into a shared memory
// region read by MirrorCam.dll (the virtual DirectShow/MF camera filter).
//
// MirrorCam.dll appears in OBS/Zoom as "AuraCastPro Virtual Camera".
// When OBS selects it, it reads frames from the shared memory at the
// stream's native frame rate.
// =============================================================================
#pragma once
#include <cstdint>
#include <atomic>
#include <memory>

struct ID3D12Resource;

namespace aura {

class VCamBridge {
public:
    VCamBridge();
    ~VCamBridge();

    void init();
    void start();
    void stop();
    void shutdown();

    // Called from render thread after each frame is composited.
    // Copies the BGRA framebuffer to shared memory for MirrorCam.dll.
    void pushFrame(const uint8_t* bgraPixels, uint32_t width, uint32_t height,
                   uint64_t timestampUs);

    // GPU readback variant -- reads NV12 texture from GPU, converts, pushes
    void pushFrameGPU(ID3D12Resource* renderTarget, uint32_t width, uint32_t height);

    bool isRunning()     const { return m_running.load(); }
    bool isClientConnected() const { return m_clientConnected.load(); }
    uint64_t framesDelivered() const { return m_frames.load(); }

private:
    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_clientConnected{false};
    std::atomic<uint64_t> m_frames{0};

    struct SharedState;
    std::unique_ptr<SharedState> m_shared;

    bool createSharedMemory(uint32_t maxWidth, uint32_t maxHeight);
};

} // namespace aura
