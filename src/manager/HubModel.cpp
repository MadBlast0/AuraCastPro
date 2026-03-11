// =============================================================================
// HubModel.cpp — QML bridge: backend state → QML property bindings
// =============================================================================
#include "../pch.h"  // PCH
#include "HubModel.h"
#include "DeviceManager.h"
#include "../licensing/LicenseManager.h"
#include <QMetaObject>
#include "NetworkStatsModel.h"
#include "../utils/DiagnosticsExporter.h"
#include "../utils/Logger.h"

#include <QDesktopServices>
#include <QUrl>
#include <QStringList>

namespace aura {

HubModel::HubModel(QObject* parent) : QObject(parent) {}

void HubModel::init(DeviceManager* devices, NetworkStatsModel* stats) {
    m_devices = devices;
    m_stats   = stats;

    if (m_devices) {
        // Reflect device state changes as mirroring / connection status
        connect(m_devices, &DeviceManager::deviceStateChanged,
                this, [this](const QString& /*deviceId*/, DeviceState state) {
            switch (state) {
                case DeviceState::Streaming:
                    setMirroring(true);
                    setStatus("MIRRORING");
                    break;
                case DeviceState::Connecting:
                case DeviceState::Connected:
                    setMirroring(false);
                    setStatus("CONNECTING");
                    break;
                case DeviceState::Pairing:
                    setMirroring(false);
                    setStatus("PAIRING");
                    break;
                default:
                    setMirroring(false);
                    setStatus("IDLE");
                    break;
            }
        });
    }

    AURA_LOG_INFO("HubModel", "Initialised. QML context bridge ready.");
}

void HubModel::shutdown() {
    setMirroring(false);
    setStatus("IDLE");
}

// ── Property setters (called by backend) ─────────────────────────────────────

void HubModel::setMirroring(bool active) {
    if (m_isMirroring.exchange(active) != active) {
        emit statusChanged();
    }
}

void HubModel::setStatus(const QString& status) {
    if (m_connectionStatus != status) {
        m_connectionStatus = status;
        emit statusChanged();
    }
}

void HubModel::appendLogLine(const QString& line) {
    m_logLines.append(line);
    while (m_logLines.size() > kMaxLogLines)
        m_logLines.removeFirst();
    m_logCacheDirty = true;
    emit logsUpdated();
}

QString HubModel::recentLogLines() const {
    if (m_logCacheDirty) {
        const_cast<HubModel*>(this)->m_cachedLogText = m_logLines.join('\n');
        const_cast<HubModel*>(this)->m_logCacheDirty = false;
    }
    return m_cachedLogText;
}

// ── Q_INVOKABLEs ─────────────────────────────────────────────────────────────

void HubModel::startMirroring() {
    AURA_LOG_INFO("HubModel", "QML requested startMirroring().");
    setStatus("CONNECTING");
    emit mirroringStartRequested();
    // DeviceManager will update state via deviceStateChanged signal
    if (m_devices) m_devices->startMirror();
}

void HubModel::stopMirroring() {
    AURA_LOG_INFO("HubModel", "QML requested stopMirroring().");
    if (m_devices) m_devices->disconnectAll();
    setMirroring(false);
    setStatus("IDLE");
    emit mirroringStopRequested();
}

void HubModel::exportLogs() {
    AURA_LOG_INFO("HubModel", "QML requested exportLogs().");
    const std::string path = DiagnosticsExporter::exportToDesktop();
    // Open the export folder in Explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
    appendLogLine(QString("[Diagnostics] Exported to: %1").arg(
        QString::fromStdString(path)));
}

void HubModel::submitPairingPin(const QString& pin) {
    AURA_LOG_INFO("HubModel", "QML submitted pairing PIN: {}.", pin.toStdString());
    emit pairingPinSubmitted(pin);
}


void HubModel::setPairingPin(const QString& pin) {
    // Called by backend when a new AirPlay pairing PIN is generated.
    // Append to the log so DiagnosticsPanel shows it, and notify QML
    // to open the pairing dialog via Main.qml's showPairingDialog().
    appendLogLine(QString("[Pairing] PIN generated: %1").arg(pin));
    AURA_LOG_INFO("HubModel", "Pairing PIN set: {}.", pin.toStdString());
}

void HubModel::toggleRecording() {
    m_isRecording = !m_isRecording;
    AURA_LOG_INFO("HubModel", "QML requested toggleRecording(). now={}",
                  m_isRecording);
    emit recordingChanged();
    emit recordingToggleRequested();
}

bool HubModel::isRecording() const {
    return m_isRecording;
}

void HubModel::activateLicense(const QString& key, const QString& email) {
    if (m_licenseManager) {
        // LicenseManager::activate(QString, QString) emits activationSucceeded
        // or activationFailed, which are connected in main.cpp to update SettingsModel
        m_licenseManager->activate(key, email);
    } else {
        AURA_LOG_WARN("HubModel", "activateLicense called but m_licenseManager is null");
    }
}

} // namespace aura
