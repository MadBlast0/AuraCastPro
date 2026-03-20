// =============================================================================
// DeviceMirrorManager.cpp -- Manages multiple device mirror windows
// =============================================================================
#include "../pch.h"
#include "DeviceMirrorManager.h"
#include "../display/DeviceMirrorWindow.h"
#include "../display/DX12Manager.h"
#include "../engine/VideoDecoder.h"
#include "../integration/StreamRecorder.h"
#include "../utils/Logger.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace aura {

DeviceMirrorManager::DeviceMirrorManager(DX12Manager* dx12, VideoDecoder* decoder)
    : m_dx12(dx12), m_decoder(decoder) {
}

DeviceMirrorManager::~DeviceMirrorManager() {
    shutdown();
}

void DeviceMirrorManager::init() {
    AURA_LOG_INFO("DeviceMirrorManager", "Initialized - ready to manage device windows");
}

void DeviceMirrorManager::shutdown() {
    AURA_LOG_INFO("DeviceMirrorManager", "Shutting down - closing {} device windows", m_devices.size());
    closeAllWindows();
}

void DeviceMirrorManager::onDeviceConnected(const std::string& deviceId, const MirrorDeviceInfo& info) {
    AURA_LOG_INFO("DeviceMirrorManager", "Device connected: {} ({})", info.deviceName, deviceId);

    // Check if device already exists
    if (m_devices.find(deviceId) != m_devices.end()) {
        AURA_LOG_WARN("DeviceMirrorManager", "Device {} already has a window", deviceId);
        return;
    }

    // Create new device entry
    DeviceEntry entry;
    entry.info = info;
    entry.active = true;

    // Create mirror window for this device
    entry.window = std::make_unique<DeviceMirrorWindow>(deviceId, info, m_dx12);
    
    // Set disconnect callback
    entry.window->setDisconnectCallback([this, deviceId](const std::string& id) {
        AURA_LOG_INFO("DeviceMirrorManager", "Disconnect requested for device: {}", id);
        onDeviceDisconnected(id);
    });

    // Set recording callback
    entry.window->setRecordingCallback([this, deviceId](const std::string& id, bool recording) {
        AURA_LOG_INFO("DeviceMirrorManager", "Recording {} for device: {}", 
                     recording ? "started" : "stopped", id);
    });

    // Initialize and start the window
    try {
        entry.window->init();
        entry.window->start();
        
        // Set window title with device name
        std::string title = info.deviceName.empty() ? 
                           ("Device " + deviceId.substr(0, 8)) : 
                           info.deviceName;
        entry.window->setTitle(title + " - AuraCastPro");
        
        AURA_LOG_INFO("DeviceMirrorManager", "Created mirror window for: {}", title);
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("DeviceMirrorManager", "Failed to create window for {}: {}", 
                      deviceId, e.what());
        return;
    }

    // Add to device map
    m_devices[deviceId] = std::move(entry);
    
    // Notify listeners
    notifyDeviceListChanged();
}

void DeviceMirrorManager::onDeviceDisconnected(const std::string& deviceId) {
    AURA_LOG_INFO("DeviceMirrorManager", "Device disconnected: {}", deviceId);

    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        AURA_LOG_WARN("DeviceMirrorManager", "Device {} not found", deviceId);
        return;
    }

    // Stop recording if active
    if (it->second.window && it->second.window->isRecording()) {
        it->second.window->stopRecording();
    }

    // Close and destroy window
    if (it->second.window) {
        it->second.window->stop();
        it->second.window->shutdown();
    }

    // Remove from map
    m_devices.erase(it);
    
    AURA_LOG_INFO("DeviceMirrorManager", "Removed device window: {}", deviceId);
    
    // Notify listeners
    notifyDeviceListChanged();
}

void DeviceMirrorManager::routeFrameToDevice(const std::string& deviceId, 
                                             ID3D12Resource* texture,
                                             uint32_t width, uint32_t height) {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end() || !it->second.window) {
        return;
    }

    // Send frame to device's window
    it->second.window->setCurrentTexture(texture);
    it->second.window->presentFrame(width, height);
}

void DeviceMirrorManager::updateDeviceStats(const std::string& deviceId, 
                                            uint32_t fps, float bitrateMbps) {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end() || !it->second.window) {
        return;
    }

    it->second.window->updateStats(fps, bitrateMbps);
}

bool DeviceMirrorManager::startRecording(const std::string& deviceId, 
                                         const RecordingConfig& config) {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end() || !it->second.window) {
        AURA_LOG_ERROR("DeviceMirrorManager", "Cannot start recording - device {} not found", deviceId);
        return false;
    }

    // Generate output path with device name
    std::string outputPath = generateRecordingPath(deviceId);
    
    return it->second.window->startRecording(outputPath, config);
}

void DeviceMirrorManager::stopRecording(const std::string& deviceId) {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end() || !it->second.window) {
        return;
    }

    it->second.window->stopRecording();
}

bool DeviceMirrorManager::isRecording(const std::string& deviceId) const {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end() || !it->second.window) {
        return false;
    }

    return it->second.window->isRecording();
}

bool DeviceMirrorManager::hasDevice(const std::string& deviceId) const {
    return m_devices.find(deviceId) != m_devices.end();
}

std::vector<std::string> DeviceMirrorManager::getDeviceIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_devices.size());
    for (const auto& [id, entry] : m_devices) {
        ids.push_back(id);
    }
    return ids;
}

MirrorDeviceInfo DeviceMirrorManager::getDeviceInfo(const std::string& deviceId) const {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        return MirrorDeviceInfo{};
    }
    return it->second.info;
}

DeviceMirrorWindow* DeviceMirrorManager::getWindow(const std::string& deviceId) {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        return nullptr;
    }
    return it->second.window.get();
}

void DeviceMirrorManager::closeWindow(const std::string& deviceId) {
    onDeviceDisconnected(deviceId);
}

void DeviceMirrorManager::closeAllWindows() {
    std::vector<std::string> deviceIds;
    for (const auto& [id, entry] : m_devices) {
        deviceIds.push_back(id);
    }

    for (const auto& id : deviceIds) {
        onDeviceDisconnected(id);
    }
}

void DeviceMirrorManager::notifyDeviceListChanged() {
    if (m_onDeviceListChanged) {
        m_onDeviceListChanged();
    }
}

std::string DeviceMirrorManager::generateRecordingPath(const std::string& deviceId) const {
    auto it = m_devices.find(deviceId);
    if (it == m_devices.end()) {
        return "recording.mp4";
    }

    // Get device name
    std::string deviceName = it->second.info.deviceName;
    if (deviceName.empty()) {
        deviceName = "Device_" + deviceId.substr(0, 8);
    }

    // Sanitize device name for filename
    for (char& c : deviceName) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            c = '_';
        }
    }

    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time_t);
    
    std::ostringstream oss;
    oss << deviceName << "_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << ".mp4";

    return oss.str();
}

} // namespace aura
