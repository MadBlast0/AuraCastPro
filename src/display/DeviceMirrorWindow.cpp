// =============================================================================
// DeviceMirrorWindow.cpp -- Individual mirror window per device
// Wraps MirrorWindow and adds per-device controls
// =============================================================================
#include "../pch.h"
#include "DeviceMirrorWindow.h"
#include "../manager/DeviceMirrorManager.h"  // For MirrorDeviceInfo
#include "MirrorWindow.h"
#include "MirrorWindowWin32.h"
#include "DX12Manager.h"
#include "../integration/StreamRecorder.h"
#include "../utils/Logger.h"

#include <CommCtrl.h>
#pragma comment(lib, "Comctl32.lib")

namespace aura {

// Internal implementation - wraps MirrorWindow
struct DeviceMirrorWindow::RenderState {
    std::unique_ptr<MirrorWindow> mirrorWindow;
};

DeviceMirrorWindow::DeviceMirrorWindow(const std::string& deviceId,
                                       const MirrorDeviceInfo& info,
                                       DX12Manager* dx12)
    : m_deviceId(deviceId)
    , m_deviceInfo(info)
    , m_dx12(dx12)
    , m_render(std::make_unique<RenderState>())
    , m_recorder(std::make_unique<StreamRecorder>()) {
    
    m_width = info.width;
    m_height = info.height;
}

DeviceMirrorWindow::~DeviceMirrorWindow() {
    shutdown();
}

void DeviceMirrorWindow::init() {
    // Create underlying mirror window
    m_render->mirrorWindow = std::make_unique<MirrorWindow>(m_dx12, nullptr);
    m_render->mirrorWindow->init();
    
    // Initialize recorder
    m_recorder->init();
}

void DeviceMirrorWindow::start() {
    if (!m_render->mirrorWindow) {
        AURA_LOG_ERROR("DeviceMirrorWindow", "Cannot start - not initialized");
        return;
    }
    
    m_render->mirrorWindow->start();
    m_running = true;
    
    // Set initial title
    std::string title = m_deviceInfo.deviceName.empty() ? 
                       ("Device " + m_deviceId.substr(0, 8)) : 
                       m_deviceInfo.deviceName;
    setTitle(title + " - AuraCastPro");
    
    // Log keyboard shortcuts
    createWin32Controls();
    
    AURA_LOG_INFO("DeviceMirrorWindow", "Started window for: {}", title);
}

void DeviceMirrorWindow::stop() {
    if (!m_running) return;
    
    // Stop recording if active
    if (m_recording) {
        stopRecording();
    }
    
    m_running = false;
}

void DeviceMirrorWindow::shutdown() {
    stop();
    
    // Cleanup controls
    destroyWin32Controls();
    
    if (m_recorder) {
        m_recorder->shutdown();
    }
    
    if (m_render->mirrorWindow) {
        m_render->mirrorWindow->shutdown();
    }
    
    AURA_LOG_INFO("DeviceMirrorWindow", "Shutdown complete for device: {}", m_deviceId);
}

void DeviceMirrorWindow::presentFrame(uint32_t width, uint32_t height) {
    if (!m_running || !m_render->mirrorWindow) return;
    
    m_render->mirrorWindow->presentFrame(width, height);
    
    // Feed to recorder if recording
    if (m_recording && m_recorder && m_currentTexture) {
        // TODO: Feed frame to recorder
        // m_recorder->feedFrame(m_currentTexture.Get(), width, height);
    }
}

void DeviceMirrorWindow::setCurrentTexture(ID3D12Resource* texture) {
    m_currentTexture = texture;
    
    if (m_render->mirrorWindow) {
        m_render->mirrorWindow->setCurrentTexture(texture);
    }
    
    // Cache dimensions
    if (texture) {
        D3D12_RESOURCE_DESC desc = texture->GetDesc();
        m_currentTextureWidth = static_cast<UINT>(desc.Width);
        m_currentTextureHeight = static_cast<UINT>(desc.Height);
    }
}

void DeviceMirrorWindow::updateStats(uint32_t fps, float bitrateMbps) {
    m_currentFps = fps;
    m_currentBitrate = bitrateMbps;
    
    // Update window title with stats
    std::string title = m_deviceInfo.deviceName.empty() ? 
                       ("Device " + m_deviceId.substr(0, 8)) : 
                       m_deviceInfo.deviceName;
    
    title += std::format(" - {}x{} @{}fps {:.1f}Mbps", 
                        m_currentTextureWidth, m_currentTextureHeight,
                        fps, bitrateMbps);
    
    if (m_recording) {
        title += " [REC]";
    }
    
    setTitle(title);
}

void DeviceMirrorWindow::setTitle(const std::string& title) {
    if (m_render->mirrorWindow) {
        m_render->mirrorWindow->setTitle(title);
    }
}

void DeviceMirrorWindow::setFullscreen(bool fs) {
    if (m_render->mirrorWindow) {
        m_render->mirrorWindow->setFullscreen(fs);
    }
}

void DeviceMirrorWindow::setAlwaysOnTop(bool onTop) {
    m_alwaysOnTop = onTop;
    // TODO: Implement always-on-top via Win32 API
    // SetWindowPos(hwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, ...)
}

void DeviceMirrorWindow::setRememberPosition(bool remember) {
    m_rememberPosition = remember;
    // TODO: Save/restore window position to settings
}

bool DeviceMirrorWindow::isFullscreen() const {
    if (m_render->mirrorWindow) {
        return m_render->mirrorWindow->isFullscreen();
    }
    return false;
}

uint32_t DeviceMirrorWindow::clientWidth() const {
    if (m_render->mirrorWindow) {
        return m_render->mirrorWindow->clientWidth();
    }
    return m_width;
}

uint32_t DeviceMirrorWindow::clientHeight() const {
    if (m_render->mirrorWindow) {
        return m_render->mirrorWindow->clientHeight();
    }
    return m_height;
}

bool DeviceMirrorWindow::startRecording(const std::string& outputPath, 
                                        const RecordingConfig& config) {
    if (m_recording) {
        AURA_LOG_WARN("DeviceMirrorWindow", "Already recording for device: {}", m_deviceId);
        return false;
    }
    
    if (!m_recorder) {
        AURA_LOG_ERROR("DeviceMirrorWindow", "Recorder not initialized");
        return false;
    }
    
    // Start recording
    if (m_recorder->startRecording(outputPath, config)) {
        m_recording = true;
        AURA_LOG_INFO("DeviceMirrorWindow", "Started recording for {}: {}", 
                     m_deviceId, outputPath);
        
        // Update title to show recording status
        updateStats(m_currentFps, m_currentBitrate);
        
        // Notify callback
        if (m_onRecording) {
            m_onRecording(m_deviceId, true);
        }
        
        return true;
    }
    
    AURA_LOG_ERROR("DeviceMirrorWindow", "Failed to start recording for: {}", m_deviceId);
    return false;
}

void DeviceMirrorWindow::stopRecording() {
    if (!m_recording) return;
    
    if (m_recorder) {
        m_recorder->stopRecording();
    }
    
    m_recording = false;
    AURA_LOG_INFO("DeviceMirrorWindow", "Stopped recording for device: {}", m_deviceId);
    
    // Update title
    updateStats(m_currentFps, m_currentBitrate);
    
    // Notify callback
    if (m_onRecording) {
        m_onRecording(m_deviceId, false);
    }
}

void DeviceMirrorWindow::toggleOverlay() {
    if (m_render->mirrorWindow) {
        m_render->mirrorWindow->toggleOverlay();
    }
}

void DeviceMirrorWindow::updateOverlayStats(uint32_t fps, float bitrateMbps) {
    // TODO: Update overlay with stats
    // This would require access to PerformanceOverlay
}

void DeviceMirrorWindow::handleDisconnectButton() {
    AURA_LOG_INFO("DeviceMirrorWindow", "Disconnect button pressed for: {}", m_deviceId);
    
    if (m_onDisconnect) {
        m_onDisconnect(m_deviceId);
    }
}

void DeviceMirrorWindow::handleRecordButton() {
    if (m_recording) {
        stopRecording();
    } else {
        // TODO: Get recording config from settings
        // For now, use default config
        RecordingConfig config;
        config.videoWidth = m_currentTextureWidth;
        config.videoHeight = m_currentTextureHeight;
        config.fps = m_currentFps;
        
        // Generate output path
        std::string outputPath = "recording_" + m_deviceId + ".mp4";
        startRecording(outputPath, config);
    }
}

void DeviceMirrorWindow::handleAlwaysOnTopButton() {
    setAlwaysOnTop(!m_alwaysOnTop);
}

void DeviceMirrorWindow::createWin32Controls() {
    // Log keyboard shortcuts for user reference
    AURA_LOG_INFO("DeviceMirrorWindow", "Keyboard shortcuts available:");
    AURA_LOG_INFO("DeviceMirrorWindow", "  R = Toggle Recording");
    AURA_LOG_INFO("DeviceMirrorWindow", "  D = Disconnect Device");
    AURA_LOG_INFO("DeviceMirrorWindow", "  F = Toggle Fullscreen");
    AURA_LOG_INFO("DeviceMirrorWindow", "  T = Toggle Always-On-Top");
    AURA_LOG_INFO("DeviceMirrorWindow", "  O = Toggle Stats Overlay");
}

void DeviceMirrorWindow::destroyWin32Controls() {
    // Cleanup if needed
}

void DeviceMirrorWindow::updateControlPositions() {
    // Update positions if needed
}

} // namespace aura
