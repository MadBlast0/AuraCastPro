#pragma once
// =============================================================================
// DeviceMirrorManager.h -- Manages multiple device mirror windows
// Creates/destroys windows per device connection/disconnection
// =============================================================================
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#include "../common/MirrorDeviceInfo.h"

namespace aura {

class DX12Manager;
class DeviceMirrorWindow;
class VideoDecoder;
struct RecordingConfig;

class DeviceMirrorManager {
public:
    using DeviceListChangedCallback = std::function<void()>;

    DeviceMirrorManager(DX12Manager* dx12, VideoDecoder* decoder);
    ~DeviceMirrorManager();

    void init();
    void shutdown();

    // Device connection management
    void onDeviceConnected(const std::string& deviceId, const MirrorDeviceInfo& info);
    void onDeviceDisconnected(const std::string& deviceId);
    
    // Video frame routing
    void routeFrameToDevice(const std::string& deviceId, ID3D12Resource* texture, 
                           uint32_t width, uint32_t height);

    // Stats updates
    void updateDeviceStats(const std::string& deviceId, uint32_t fps, float bitrateMbps);

    // Recording control
    bool startRecording(const std::string& deviceId, const RecordingConfig& config);
    void stopRecording(const std::string& deviceId);
    bool isRecording(const std::string& deviceId) const;

    // Device queries
    bool hasDevice(const std::string& deviceId) const;
    int deviceCount() const { return static_cast<int>(m_devices.size()); }
    std::vector<std::string> getDeviceIds() const;
    MirrorDeviceInfo getDeviceInfo(const std::string& deviceId) const;

    // Window management
    DeviceMirrorWindow* getWindow(const std::string& deviceId);
    void closeWindow(const std::string& deviceId);
    void closeAllWindows();

    // Callbacks
    void setDeviceListChangedCallback(DeviceListChangedCallback cb) { 
        m_onDeviceListChanged = cb; 
    }

private:
    DX12Manager* m_dx12;
    VideoDecoder* m_decoder;

    struct DeviceEntry {
        std::unique_ptr<DeviceMirrorWindow> window;
        MirrorDeviceInfo info;
        bool active{true};
    };

    std::unordered_map<std::string, DeviceEntry> m_devices;
    DeviceListChangedCallback m_onDeviceListChanged;

    void notifyDeviceListChanged();
    std::string generateRecordingPath(const std::string& deviceId) const;
};

} // namespace aura
