// =============================================================================
// DeviceManager.cpp — Device lifecycle management
// =============================================================================

#include "../pch.h"  // PCH
#include "DeviceManager.h"
#include "../utils/Logger.h"

#include <algorithm>

namespace aura {

DeviceManager::DeviceManager(QObject* parent) : QObject(parent) {
    qRegisterMetaType<DeviceInfo>();
    qRegisterMetaType<DeviceState>();
}

void DeviceManager::init() {
    AURA_LOG_INFO("DeviceManager", "Initialised. Device list empty — waiting for discovery.");
}

void DeviceManager::shutdown() {
    AURA_LOG_INFO("DeviceManager", "Shutdown. {} devices in list.", m_devices.size());
    m_devices.clear();
    emit devicesChanged();
}

int DeviceManager::connectedCount() const {
    return static_cast<int>(std::count_if(m_devices.begin(), m_devices.end(),
        [](const DeviceInfo& d) {
            return d.state == DeviceState::Connected ||
                   d.state == DeviceState::Streaming;
        }));
}

void DeviceManager::onDeviceDiscovered(const DeviceInfo& info) {
    // Check if we already know this device
    auto* existing = findDeviceMutable(info.id);
    if (existing) {
        existing->lastSeen   = QDateTime::currentDateTime();
        existing->ipAddress  = info.ipAddress;
        existing->name       = info.name;
        emit devicesChanged();
        AURA_LOG_DEBUG("DeviceManager", "Updated known device: {} ({})", 
                       info.name.toStdString(), info.ipAddress.toStdString());
        return;
    }

    DeviceInfo newDevice = info;
    newDevice.lastSeen = QDateTime::currentDateTime();
    m_devices.append(newDevice);

    AURA_LOG_INFO("DeviceManager", "New device discovered: {} [{}] at {}",
                  info.name.toStdString(),
                  typeToString(info.type).toStdString(),
                  info.ipAddress.toStdString());

    emit deviceAdded(info.id);
    emit devicesChanged();
}

void DeviceManager::onDeviceStateChanged(const QString& deviceId, DeviceState newState) {
    auto* device = findDeviceMutable(deviceId);
    if (!device) {
        AURA_LOG_WARN("DeviceManager", "State change for unknown device: {}",
                      deviceId.toStdString());
        return;
    }

    const DeviceState oldState = device->state;
    device->state = newState;

    if (newState == DeviceState::Connected || newState == DeviceState::Streaming) {
        device->lastConnected = QDateTime::currentDateTime();
    }

    AURA_LOG_INFO("DeviceManager", "Device '{}': {} → {}",
                  device->name.toStdString(),
                  stateToString(oldState).toStdString(),
                  stateToString(newState).toStdString());

    emit deviceStateChanged(deviceId, newState);
    emit devicesChanged();
}

void DeviceManager::onDevicePaired(const QString& deviceId) {
    auto* device = findDeviceMutable(deviceId);
    if (!device) return;
    device->isPaired = true;
    device->state    = DeviceState::Paired;
    emit devicesChanged();
    AURA_LOG_INFO("DeviceManager", "Device '{}' pairing complete.",
                  device->name.toStdString());
}

void DeviceManager::connectDevice(const QString& deviceId) {
    const auto* device = findDevice(deviceId);
    if (!device) return;
    AURA_LOG_INFO("DeviceManager", "Connecting to '{}'...", device->name.toStdString());
    onDeviceStateChanged(deviceId, DeviceState::Connecting);
    // Trigger mirroring via the mirrorRequested signal
    // (main.cpp wires this to ADBBridge or the relevant protocol host)
    emit mirrorRequested(deviceId);
    AURA_LOG_INFO("DeviceManager", "Mirror requested for: {}", device->name.toStdString());
}

void DeviceManager::disconnectDevice(const QString& deviceId) {
    const auto* device = findDevice(deviceId);
    if (!device) return;
    AURA_LOG_INFO("DeviceManager", "Disconnecting '{}'.", device->name.toStdString());
    onDeviceStateChanged(deviceId, DeviceState::Disconnected);
}

void DeviceManager::disconnectAll() {
    QStringList ids;
    for (const auto& d : m_devices) ids << d.id;
    for (const auto& id : ids) disconnectDevice(id);
    AURA_LOG_INFO("DeviceManager", "All devices disconnected.");
}

void DeviceManager::forgetDevice(const QString& deviceId) {
    disconnectDevice(deviceId);
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(),
        [&](const DeviceInfo& d) { return d.id == deviceId; }),
        m_devices.end());
    emit deviceRemoved(deviceId);
    emit devicesChanged();
    AURA_LOG_INFO("DeviceManager", "Forgot device: {}", deviceId.toStdString());
}

const DeviceInfo* DeviceManager::findDevice(const QString& deviceId) const {
    for (const auto& d : m_devices) {
        if (d.id == deviceId) return &d;
    }
    return nullptr;
}

DeviceInfo* DeviceManager::findDeviceMutable(const QString& deviceId) {
    for (auto& d : m_devices) {
        if (d.id == deviceId) return &d;
    }
    return nullptr;
}

QString DeviceManager::stateToString(DeviceState state) {
    switch (state) {
        case DeviceState::Discovered:   return "Discovered";
        case DeviceState::Pairing:      return "Pairing";
        case DeviceState::Paired:       return "Paired";
        case DeviceState::Connecting:   return "Connecting";
        case DeviceState::Connected:    return "Connected";
        case DeviceState::Streaming:    return "Streaming";
        case DeviceState::Disconnected: return "Disconnected";
    }
    return "Unknown";
}

QString DeviceManager::typeToString(DeviceType type) {
    switch (type) {
        case DeviceType::IOS:     return "iOS";
        case DeviceType::Android: return "Android";
        case DeviceType::MacOS:   return "macOS";
        default:                  return "Unknown";
    }
}

// ── startScan() — trigger mDNS re-scan ───────────────────────────────────────
// The actual scan is owned by MDNSService; DeviceManager just signals the intent.
// MDNSService connects to this signal in main.cpp.
void DeviceManager::startScan() {
    AURA_LOG_INFO("DeviceManager", "Scan requested by UI.");
    emit scanRequested();
}

// ── startMirror() — begin mirroring from first connected device ───────────────
void DeviceManager::startMirror() {
    for (const auto& d : m_devices) {
        if (d.state == DeviceState::Connected || d.state == DeviceState::Streaming) {
            AURA_LOG_INFO("DeviceManager", "startMirror → device {}", d.id.toStdString());
            emit mirrorRequested(d.id);
            return;
        }
    }
    AURA_LOG_WARN("DeviceManager", "startMirror: no connected device found.");
}

} // namespace aura

#include "DeviceManager.moc"
