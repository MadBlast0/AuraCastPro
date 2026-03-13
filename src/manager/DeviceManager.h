#pragma once
// =============================================================================
// DeviceManager.h -- Device lifecycle and state machine.
//
// Tracks every discovered, paired, and connected device.
// Emits Qt signals when devices are added, removed, or change state.
// The UI and protocol hosts listen to these signals.
//
// State machine per device:
//   Discovered -> Pairing -> Paired -> Connecting -> Connected -> Streaming
//   Any state -> Disconnected -> (removed after timeout)
// =============================================================================

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

namespace aura {

enum class DeviceState {
    Discovered,   // Seen via mDNS -- not yet paired
    Pairing,      // Pairing in progress (PIN entry)
    Paired,       // Pairing complete -- has stored key
    Connecting,   // TCP/ADB connection being established
    Connected,    // Control channel open, no video yet
    Streaming,    // Video is flowing
    Disconnected  // Lost connection
};

enum class DeviceType { IOS, Android, MacOS, Unknown };

struct DeviceInfo {
    Q_GADGET                          // enables QML property access for this value type

    // QML-accessible properties (read from model.fieldName in ListView delegates)
    Q_PROPERTY(QString     deviceId         MEMBER id)
    Q_PROPERTY(QString     name             MEMBER name)
    Q_PROPERTY(QString     ipAddress        MEMBER ipAddress)
    Q_PROPERTY(QString     modelIdentifier  MEMBER modelIdentifier)
    Q_PROPERTY(QString     osVersion        MEMBER osVersion)
    Q_PROPERTY(bool        isPaired         MEMBER isPaired)
    Q_PROPERTY(bool        isConnected      READ   isConnected)
    Q_PROPERTY(bool        isStreaming      READ   isStreaming)
    Q_PROPERTY(QString     protocol         READ   protocolString)
    Q_PROPERTY(int         signalStrengthDbm MEMBER signalStrengthDbm)

public:
    QString     id;           // Unique stable ID (MAC or serial)
    QString     name;         // Human-readable name ("Mia's iPhone 15")
    QString     ipAddress;    // Current IP (empty for ADB/USB)
    DeviceType  type{DeviceType::Unknown};
    DeviceState state{DeviceState::Discovered};
    QString     modelIdentifier;  // e.g. "iPhone16,1"
    QString     osVersion;        // e.g. "iOS 17.2"
    bool        isPaired{false};
    QDateTime   lastSeen;
    QDateTime   lastConnected;
    int         signalStrengthDbm{0}; // Wi-Fi RSSI (0 = USB/unknown)

    bool isConnected() const {
        return state == DeviceState::Connected || state == DeviceState::Streaming;
    }
    bool isStreaming() const { return state == DeviceState::Streaming; }
    QString protocolString() const {
        switch (type) {
        case DeviceType::IOS:     return "AirPlay";
        case DeviceType::Android: return "ADB";
        case DeviceType::MacOS:   return "Cast";
        default:                  return "Unknown";
        }
    }
};

class DeviceManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QList<DeviceInfo> devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY devicesChanged)

public:
    explicit DeviceManager(QObject* parent = nullptr);

    void init();
    void shutdown();

    QList<DeviceInfo> devices() const { return m_devices; }
    int connectedCount() const;

    // Called by discovery layer when a new device appears on the network
    void onDeviceDiscovered(const DeviceInfo& info);

    // Called by protocol hosts when connection state changes
    void onDeviceStateChanged(const QString& deviceId, DeviceState newState);

    // Called by SecurityVault when pairing is complete
    void onDevicePaired(const QString& deviceId);

    // Trigger connection to a specific device
    Q_INVOKABLE void connectDevice(const QString& deviceId);

    // Disconnect and optionally forget (remove pairing)
    Q_INVOKABLE void disconnectDevice(const QString& deviceId);
    Q_INVOKABLE void forgetDevice(const QString& deviceId);

    // Trigger a fresh mDNS/Bonjour scan for nearby devices
    Q_INVOKABLE void startScan();

    // Start mirroring from the first connected device
    Q_INVOKABLE void startMirror();
    Q_INVOKABLE void disconnectAll();

    // Find device by ID (returns nullptr if not found)
    const DeviceInfo* findDevice(const QString& deviceId) const;

    static QString stateToString(DeviceState state);
    static QString typeToString(DeviceType type);

signals:
    void devicesChanged();
    void deviceAdded(const QString& deviceId);
    void deviceRemoved(const QString& deviceId);
    void deviceStateChanged(const QString& deviceId, DeviceState state);
    void devicePairingRequested(const QString& deviceId, const QString& pin);
    void scanRequested();
    void mirrorRequested(const QString& deviceId);

private:
    QList<DeviceInfo> m_devices;
    DeviceInfo* findDeviceMutable(const QString& deviceId);
};

} // namespace aura

// Make DeviceInfo usable in Qt meta-system
Q_DECLARE_METATYPE(aura::DeviceInfo)
Q_DECLARE_METATYPE(aura::DeviceState)
