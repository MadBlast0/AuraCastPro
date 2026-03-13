#pragma once
// =============================================================================
// HubModel.h -- QML-facing controller for the hub (main) window
//
// Exposes the core application state the QML UI needs:
//   • isMirroring        -- true when a device is actively streaming
//   • connectionStatus   -- "IDLE" / "CONNECTING" / "MIRRORING" / "ERROR"
//   • recentLogLines     -- last N log lines for DiagnosticsPanel
//   • startMirroring()   -- Q_INVOKABLE
//   • stopMirroring()    -- Q_INVOKABLE
//   • exportLogs()       -- Q_INVOKABLE (calls DiagnosticsExporter)
//   • submitPairingPin() -- Q_INVOKABLE
// =============================================================================
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>

namespace aura {

class DeviceManager;
class LicenseManager;
class NetworkStatsModel;
class DiagnosticsExporter;

class HubModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool    isMirroring       READ isMirroring       NOTIFY statusChanged)
    Q_PROPERTY(QString connectionStatus  READ connectionStatus  NOTIFY statusChanged)
    Q_PROPERTY(QString recentLogLines    READ recentLogLines    NOTIFY logsUpdated)
    Q_PROPERTY(bool    isRecording        READ isRecording       NOTIFY recordingChanged)

public:
    explicit HubModel(QObject* parent = nullptr);

    void init(DeviceManager* devices, NetworkStatsModel* stats);
    void setLicenseManager(LicenseManager* lm) { m_licenseManager = lm; }
    void shutdown();

    // ── Property getters ──────────────────────────────────────────────────────
    bool    isMirroring()      const { return m_isMirroring.load(); }
    QString connectionStatus() const { return m_connectionStatus; }
    QString recentLogLines()   const;

    // ── Called by backend when state changes ──────────────────────────────────
    void setMirroring(bool active);
    void setStatus(const QString& status);
    void appendLogLine(const QString& line);
    void setPairingPin(const QString& pin);

public slots:
    // ── Q_INVOKABLEs called from QML ─────────────────────────────────────────
    Q_INVOKABLE void startMirroring();
    Q_INVOKABLE void stopMirroring();
    Q_INVOKABLE void exportLogs();
    Q_INVOKABLE void submitPairingPin(const QString& pin);
    Q_INVOKABLE void toggleRecording();
    // License activation -- called from QML SettingsPage
    Q_INVOKABLE void activateLicense(const QString& key, const QString& email);
    Q_INVOKABLE bool isRecording() const;

signals:
    void statusChanged();
    void logsUpdated();
    void mirroringStartRequested();
    void mirroringStopRequested();
    void recordingToggleRequested();
    void pairingPinSubmitted(const QString& pin);
    void recordingChanged();

    // Emitted by C++ (AirPlay2Host) after PIN verification completes.
    // QML PairingDialog connects: hubModel.onPairingResult -> showSuccess/showFailed
    void pairingResult(bool success);

public slots:
    // Called by C++ protocol layer when pairing verification finishes
    void notifyPairingResult(bool success) { emit pairingResult(success); }

private:
    DeviceManager*     m_devices{nullptr};
    LicenseManager*    m_licenseManager{nullptr};
    NetworkStatsModel* m_stats{nullptr};

    std::atomic<bool> m_isMirroring{false};
    bool              m_isRecording{false};
    QString           m_connectionStatus{"IDLE"};

    // Ring buffer of recent log lines (last 200 lines)
    static constexpr int kMaxLogLines = 200;
    QStringList       m_logLines;
    QString           m_cachedLogText;
    bool              m_logCacheDirty{true};
};

} // namespace aura
