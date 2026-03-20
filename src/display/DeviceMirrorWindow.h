#pragma once
// =============================================================================
// DeviceMirrorWindow.h -- Individual mirror window for each connected device
// Includes per-device controls: recording, disconnect, stats, etc.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <cstdint>
#include <functional>

#include "../common/MirrorDeviceInfo.h"

using Microsoft::WRL::ComPtr;

namespace aura {

class DX12Manager;
class MirrorWindowWin32;
class PerformanceOverlay;
class StreamRecorder;
struct RecordingConfig;

class DeviceMirrorWindow {
public:
    using DisconnectCallback = std::function<void(const std::string& deviceId)>;
    using RecordingCallback = std::function<void(const std::string& deviceId, bool recording)>;

    DeviceMirrorWindow(const std::string& deviceId, 
                       const MirrorDeviceInfo& info,
                       DX12Manager* dx12);
    ~DeviceMirrorWindow();

    void init();
    void start();
    void stop();
    void shutdown();

    // Video frame presentation
    void presentFrame(uint32_t width, uint32_t height);
    void setCurrentTexture(ID3D12Resource* texture);

    // Device info
    std::string deviceId() const { return m_deviceId; }
    std::string deviceName() const { return m_deviceInfo.deviceName; }
    MirrorDeviceInfo deviceInfo() const { return m_deviceInfo; }
    
    // Update stats
    void updateStats(uint32_t fps, float bitrateMbps);

    // Window controls
    void setTitle(const std::string& title);
    void setFullscreen(bool fs);
    void setAlwaysOnTop(bool onTop);
    void setRememberPosition(bool remember);
    
    bool isVisible() const { return m_running; }
    bool isRecording() const { return m_recording; }
    bool isFullscreen() const;
    bool isAlwaysOnTop() const { return m_alwaysOnTop; }
    bool rememberPosition() const { return m_rememberPosition; }

    uint32_t clientWidth() const;
    uint32_t clientHeight() const;

    // Recording control
    bool startRecording(const std::string& outputPath, const RecordingConfig& config);
    void stopRecording();

    // Callbacks
    void setDisconnectCallback(DisconnectCallback cb) { m_onDisconnect = cb; }
    void setRecordingCallback(RecordingCallback cb) { m_onRecording = cb; }

    // Overlay controls
    void setOverlay(PerformanceOverlay* overlay) { m_overlay = overlay; }
    void toggleOverlay();
    void updateOverlayStats(uint32_t fps, float bitrateMbps);

private:
    std::string m_deviceId;
    MirrorDeviceInfo m_deviceInfo;
    DX12Manager* m_dx12;
    PerformanceOverlay* m_overlay{nullptr};

    std::unique_ptr<StreamRecorder> m_recorder;

    bool m_running{false};
    bool m_recording{false};
    bool m_alwaysOnTop{false};
    bool m_rememberPosition{false};
    
    uint32_t m_width{1280};
    uint32_t m_height{720};
    uint32_t m_currentFps{0};
    float m_currentBitrate{0.0f};

    ComPtr<ID3D12Resource> m_currentTexture;
    UINT m_currentTextureWidth{1920};
    UINT m_currentTextureHeight{1080};

    DisconnectCallback m_onDisconnect;
    RecordingCallback m_onRecording;

    struct RenderState;
    std::unique_ptr<RenderState> m_render;

    void createSwapchainAndResources();
    void onResize(uint32_t w, uint32_t h);
    void createControlOverlay();
    void handleDisconnectButton();
    void handleRecordButton();
    void handleFullscreenButton();
    void handleAlwaysOnTopButton();
    
    // Win32 UI controls
    void createWin32Controls();
    void destroyWin32Controls();
    void updateControlPositions();
    
    HWND m_hwndRecordBtn{nullptr};
    HWND m_hwndDisconnectBtn{nullptr};
    HWND m_hwndFullscreenBtn{nullptr};
    HWND m_hwndStatsLabel{nullptr};
};

} // namespace aura
