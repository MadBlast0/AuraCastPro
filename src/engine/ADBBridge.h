#pragma once
// =============================================================================
// ADBBridge.h -- Android Debug Bridge wired/wireless mirroring
// =============================================================================
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>

namespace aura {

struct AndroidDevice {
    std::string serial;      // e.g. "192.168.1.100:5555" or "RFXXXXX"
    std::string model;       // e.g. "SM-S918B"
    std::string androidVer;  // e.g. "14"
    bool        authorized{false};
    bool        isWifi{false};
    uint32_t    screenWidth{0};
    uint32_t    screenHeight{0};
};

class ADBBridge {
public:
    using DeviceFoundCallback      = std::function<void(const AndroidDevice&)>;
    using DeviceLostCallback       = std::function<void(const std::string& serial)>;
    using StreamCallback           = std::function<void(const uint8_t* data, size_t len,
                                                         uint64_t pts)>;
    // Fired when a device moves from unauthorized -> authorized (USB debug dialog accepted)
    // or when adb wireless pairing completes.  success=true on authorize, false on rejection.
    using PairingResultCallback    = std::function<void(const std::string& serial, bool success)>;
    ADBBridge();
    ~ADBBridge();

    void init();
    void start();
    void stop();
    void shutdown();

    // Scan for connected devices
    std::vector<AndroidDevice> enumerateDevices();
    void scanForDevices() { enumerateDevices(); }

    // Start mirroring a specific device
    bool startMirroring(const std::string& serial, StreamCallback videoCallback);

    // Allow/disallow new ADB mirroring sessions from being started
    void setMirroringActive(bool active) { m_mirroringActive.store(active, std::memory_order_relaxed); }
    void stopMirroring(const std::string& serial);

    // Wireless ADB pairing: runs `adb pair <address>:<port> <pairingCode>`
    // address is IP:port as shown on the Android device in Developer Options.
    // Returns true if the adb command succeeded (device added to known hosts).
    bool pairWireless(const std::string& addressAndPort, const std::string& pairingCode);

    void setDeviceFoundCallback(DeviceFoundCallback cb)     { m_onFound        = std::move(cb); }
    void setDeviceLostCallback(DeviceLostCallback cb)      { m_onLost         = std::move(cb); }
    void setPairingResultCallback(PairingResultCallback cb){ m_onPairingResult = std::move(cb); }

    bool isADBAvailable() const { return m_adbAvailable; }

private:
    bool        m_adbAvailable{false};
    std::string m_adbExe{"adb"};  // Task 216: path to adb.exe (bundled or PATH)
    std::atomic<bool> m_running{false};
    std::thread       m_scanThread;
    DeviceFoundCallback    m_onFound;
    DeviceLostCallback     m_onLost;
    PairingResultCallback  m_onPairingResult;
    std::atomic<bool>      m_mirroringActive{true};

    struct MirrorSession;
    std::vector<std::unique_ptr<MirrorSession>> m_sessions;

    std::string runADB(const std::string& args) const;
    bool runADBDetached(const std::string& args) const;
    bool findADB();
    void scanLoop();
};

} // namespace aura
