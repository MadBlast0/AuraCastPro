#pragma once
// =============================================================================
// SettingsModel.h — Complete application settings (45 Q_PROPERTYs).
// FIXED: Added all properties referenced by QML but previously missing:
//   displayName, firstRunCompleted, airplayPort, castPort, rtmpUrl, streamKey,
//   telemetryEnabled, hardwareDecodeEnabled, audioEnabled, micEnabled,
//   virtualAudioEnabled, virtualCameraEnabled, recordingFolder, alwaysOnTop,
//   startWithWindows, minimiseToTray, showOverlayOnStart, gpuFrameBufferCount,
//   networkThreadCount, logLevel, streamBitrateKbps, encoderPreference,
//   avSyncOffsetMs, deviceVolume, micVolume, outputDeviceId, recordingQuality,
//   selectedNetworkIface, preferredCodec, hardwareDecodeEnabled,
//   maxResolutionIndex, fpsCapIndex
//
// Settings file: %APPDATA%\AuraCastPro\settings.json
// Schema versioned — missing keys load as defaults (safe for upgrades).
// =============================================================================

#include <QObject>
#include <QString>

// Forward declaration — SecurityVault is injected after vault init
namespace aura { class SecurityVault; }

namespace aura {

class SettingsModel : public QObject {
    Q_OBJECT

    // ── Identity ──────────────────────────────────────────────────────────────
    Q_PROPERTY(QString displayName       READ displayName       WRITE setDisplayName       NOTIFY identityChanged)
    Q_PROPERTY(bool firstRunCompleted    READ firstRunCompleted WRITE setFirstRunCompleted NOTIFY identityChanged)

    // ── Video ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(int  maxWidth             READ maxWidth             WRITE setMaxWidth             NOTIFY videoChanged)
    Q_PROPERTY(int  maxHeight            READ maxHeight            WRITE setMaxHeight            NOTIFY videoChanged)
    Q_PROPERTY(int  maxFps               READ maxFps               WRITE setMaxFps               NOTIFY videoChanged)
    Q_PROPERTY(int  maxResolutionIndex   READ maxResolutionIndex   WRITE setMaxResolutionIndex   NOTIFY videoChanged)
    Q_PROPERTY(int  fpsCapIndex          READ fpsCapIndex          WRITE setFpsCapIndex          NOTIFY videoChanged)
    Q_PROPERTY(bool hdrEnabled           READ hdrEnabled           WRITE setHdrEnabled           NOTIFY videoChanged)
    Q_PROPERTY(bool hardwareDecodeEnabled READ hardwareDecodeEnabled WRITE setHardwareDecodeEnabled NOTIFY videoChanged)
    Q_PROPERTY(QString preferredCodec    READ preferredCodec       WRITE setPreferredCodec       NOTIFY videoChanged)

    // ── Audio ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(bool    audioEnabled        READ audioEnabled        WRITE setAudioEnabled        NOTIFY audioChanged)
    Q_PROPERTY(bool    micEnabled          READ micEnabled          WRITE setMicEnabled          NOTIFY audioChanged)
    Q_PROPERTY(bool    virtualAudioEnabled READ virtualAudioEnabled WRITE setVirtualAudioEnabled NOTIFY audioChanged)
    Q_PROPERTY(QString outputDeviceId      READ outputDeviceId      WRITE setOutputDeviceId      NOTIFY audioChanged)
    Q_PROPERTY(double  deviceVolume        READ deviceVolume        WRITE setDeviceVolume        NOTIFY audioChanged)
    Q_PROPERTY(double  micVolume           READ micVolume           WRITE setMicVolume           NOTIFY audioChanged)
    Q_PROPERTY(int     avSyncOffsetMs      READ avSyncOffsetMs      WRITE setAvSyncOffsetMs      NOTIFY audioChanged)

    // ── Network ───────────────────────────────────────────────────────────────
    Q_PROPERTY(int     maxBitrateKbps      READ maxBitrateKbps      WRITE setMaxBitrateKbps      NOTIFY networkChanged)
    Q_PROPERTY(bool    fecEnabled          READ fecEnabled          WRITE setFecEnabled          NOTIFY networkChanged)
    Q_PROPERTY(int     jitterBufferMs      READ jitterBufferMs      WRITE setJitterBufferMs      NOTIFY networkChanged)
    Q_PROPERTY(int     airplayPort         READ airplayPort         WRITE setAirplayPort         NOTIFY networkChanged)
    Q_PROPERTY(int     castPort            READ castPort            WRITE setCastPort            NOTIFY networkChanged)
    Q_PROPERTY(QString selectedNetworkIface READ selectedNetworkIface WRITE setSelectedNetworkIface NOTIFY networkChanged)
    Q_PROPERTY(int     networkThreadCount  READ networkThreadCount  WRITE setNetworkThreadCount  NOTIFY networkChanged)

    // ── Recording ─────────────────────────────────────────────────────────────
    Q_PROPERTY(QString recordingFolder   READ recordingFolder   WRITE setRecordingFolder   NOTIFY recordingChanged)
    Q_PROPERTY(QString recordingQuality  READ recordingQuality  WRITE setRecordingQuality  NOTIFY recordingChanged)
    Q_PROPERTY(bool    autoRecord        READ autoRecord        WRITE setAutoRecord        NOTIFY recordingChanged)

    // ── Streaming ─────────────────────────────────────────────────────────────
    Q_PROPERTY(QString rtmpUrl             READ rtmpUrl             WRITE setRtmpUrl             NOTIFY streamingChanged)
    Q_PROPERTY(QString streamKey           READ streamKey           WRITE setStreamKey           NOTIFY streamingChanged)
    Q_PROPERTY(QString encoderPreference   READ encoderPreference   WRITE setEncoderPreference   NOTIFY streamingChanged)
    Q_PROPERTY(int     streamBitrateKbps   READ streamBitrateKbps   WRITE setStreamBitrateKbps   NOTIFY streamingChanged)
    Q_PROPERTY(bool virtualCameraEnabled   READ virtualCameraEnabled WRITE setVirtualCameraEnabled NOTIFY streamingChanged)

    // ── UI ────────────────────────────────────────────────────────────────────
    Q_PROPERTY(bool    showOverlay        READ showOverlay        WRITE setShowOverlay        NOTIFY uiChanged)
    Q_PROPERTY(bool    darkMode           READ darkMode           WRITE setDarkMode           NOTIFY uiChanged)
    Q_PROPERTY(bool    alwaysOnTop        READ alwaysOnTop        WRITE setAlwaysOnTop        NOTIFY uiChanged)
    Q_PROPERTY(bool    startWithWindows   READ startWithWindows   WRITE setStartWithWindows   NOTIFY uiChanged)
    Q_PROPERTY(bool    minimiseToTray     READ minimiseToTray     WRITE setMinimiseToTray     NOTIFY uiChanged)
    Q_PROPERTY(bool    showOverlayOnStart READ showOverlayOnStart WRITE setShowOverlayOnStart NOTIFY uiChanged)
    Q_PROPERTY(QString language           READ language           WRITE setLanguage           NOTIFY uiChanged)

    // ── SettingsPage display aliases ──────────────────────────────────────
    // These map the flat QML key names to existing properties (read-only)
    Q_PROPERTY(int     targetFps         READ maxFps               NOTIFY videoChanged)
    Q_PROPERTY(QString qualityPreset     READ recordingQuality     NOTIFY recordingChanged)
    Q_PROPERTY(bool    hwAccel           READ hardwareDecodeEnabled NOTIFY videoChanged)
    Q_PROPERTY(bool    hdrOutput         READ hdrEnabled           NOTIFY videoChanged)
    Q_PROPERTY(QString audioDevice       READ outputDeviceId       NOTIFY audioChanged)
    Q_PROPERTY(bool    virtualAudio      READ virtualAudioEnabled  NOTIFY audioChanged)
    Q_PROPERTY(QString outputFolder      READ recordingFolder      NOTIFY recordingChanged)
    Q_PROPERTY(QString recordFormat      READ recordingQuality     NOTIFY recordingChanged)
    Q_PROPERTY(int     diskWarnGb        READ diskWarnGb           NOTIFY recordingChanged)
    // SettingsPage NETWORK section aliases
    Q_PROPERTY(QString networkInterface  READ selectedNetworkIface NOTIFY networkChanged)
    Q_PROPERTY(double  maxBitrateMbps    READ maxBitrateMbps       NOTIFY networkChanged)
    // License info — provided by LicenseManager, mirrored here for QML
    Q_PROPERTY(QString licenseKey     READ licenseKey     WRITE setLicenseKey     NOTIFY licenseChanged)
    Q_PROPERTY(QString edition        READ edition        NOTIFY licenseChanged)
    Q_PROPERTY(QString licenseStatus  READ licenseStatus  NOTIFY licenseChanged)
    // System info (read-only)
    Q_PROPERTY(QString appVersion  READ appVersion  CONSTANT)
    Q_PROPERTY(QString buildDate   READ buildDate   CONSTANT)
    Q_PROPERTY(QString gpuName     READ gpuName     WRITE setGpuName  NOTIFY systemInfoChanged)
    Q_PROPERTY(QString osVersion   READ osVersion   WRITE setOsVersion NOTIFY systemInfoChanged)

    // ── Advanced ──────────────────────────────────────────────────────────────
    Q_PROPERTY(int     gpuFrameBufferCount READ gpuFrameBufferCount WRITE setGpuFrameBufferCount NOTIFY advancedChanged)
    Q_PROPERTY(QString logLevel            READ logLevel            WRITE setLogLevel            NOTIFY advancedChanged)

    // ── Privacy ───────────────────────────────────────────────────────────────
    Q_PROPERTY(bool telemetryEnabled     READ telemetryEnabled    WRITE setTelemetryEnabled    NOTIFY privacyChanged)

public:
    explicit SettingsModel(QObject* parent = nullptr);

    void load();
    void setVault(SecurityVault* vault);  // Inject vault after it is initialised in main()
    Q_INVOKABLE void save() const;
    Q_INVOKABLE void resetToDefaults();

    // ── Getters ───────────────────────────────────────────────────────────────
    QString displayName()          const { return m_displayName; }
    bool    firstRunCompleted()    const { return m_firstRunCompleted; }

    int     maxWidth()             const { return m_maxWidth; }
    int     maxHeight()            const { return m_maxHeight; }
    int     maxFps()               const { return m_maxFps; }
    int     maxResolutionIndex()   const { return m_maxResolutionIndex; }
    int     fpsCapIndex()          const { return m_fpsCapIndex; }
    bool    hdrEnabled()           const { return m_hdrEnabled; }
    bool    hardwareDecodeEnabled() const { return m_hardwareDecodeEnabled; }
    QString preferredCodec()       const { return m_preferredCodec; }

    bool    audioEnabled()         const { return m_audioEnabled; }
    bool    micEnabled()           const { return m_micEnabled; }
    bool    virtualAudioEnabled()  const { return m_virtualAudioEnabled; }
    QString outputDeviceId()       const { return m_outputDeviceId; }
    double  deviceVolume()         const { return m_deviceVolume; }
    double  micVolume()            const { return m_micVolume; }
    int     avSyncOffsetMs()       const { return m_avSyncOffsetMs; }

    int     maxBitrateKbps()       const { return m_maxBitrateKbps; }
    double  maxBitrateMbps()       const { return m_maxBitrateKbps / 1000.0; }  // alias for SettingsPage
    bool    fecEnabled()           const { return m_fecEnabled; }
    int     jitterBufferMs()       const { return m_jitterBufferMs; }
    int     airplayPort()          const { return m_airplayPort; }
    int     castPort()             const { return m_castPort; }
    QString selectedNetworkIface() const { return m_selectedNetworkIface; }
    int     networkThreadCount()   const { return m_networkThreadCount; }

    QString recordingFolder()      const { return m_recordingFolder; }
    QString recordingQuality()     const { return m_recordingQuality; }
    bool    autoRecord()           const { return m_autoRecord; }

    QString rtmpUrl()              const { return m_rtmpUrl; }
    QString streamKey() const {
        if (m_vault) {
            auto s = m_vault->retrieve("streamKey");
            return s ? QString::fromStdString(*s) : QString{};
        }
        return m_streamKey;  // fallback before vault is ready
    }
    QString encoderPreference()    const { return m_encoderPreference; }
    int     streamBitrateKbps()    const { return m_streamBitrateKbps; }
    bool    virtualCameraEnabled() const { return m_virtualCameraEnabled; }

    bool    showOverlay()          const { return m_showOverlay; }
    bool    darkMode()             const { return m_darkMode; }
    bool    alwaysOnTop()          const { return m_alwaysOnTop; }
    bool    startWithWindows()     const { return m_startWithWindows; }
    bool    minimiseToTray()       const { return m_minimiseToTray; }
    bool    showOverlayOnStart()   const { return m_showOverlayOnStart; }
    QString language()             const { return m_language; }

    int     gpuFrameBufferCount()  const { return m_gpuFrameBufferCount; }
    QString logLevel()             const { return m_logLevel; }

    bool    telemetryEnabled()     const { return m_telemetryEnabled; }

    // ── Setters ───────────────────────────────────────────────────────────────
    void setDisplayName(const QString& v);
    void setFirstRunCompleted(bool v);

    void setMaxWidth(int v);
    void setMaxHeight(int v);
    void setMaxFps(int v);
    void setMaxResolutionIndex(int v);
    void setFpsCapIndex(int v);
    void setHdrEnabled(bool v);
    void setHardwareDecodeEnabled(bool v);
    void setPreferredCodec(const QString& v);

    void setAudioEnabled(bool v);
    void setMicEnabled(bool v);
    void setVirtualAudioEnabled(bool v);
    void setOutputDeviceId(const QString& v);
    void setDeviceVolume(double v);
    void setMicVolume(double v);
    void setAvSyncOffsetMs(int v);

    void setMaxBitrateKbps(int v);
    void setFecEnabled(bool v);
    void setJitterBufferMs(int v);
    void setAirplayPort(int v);
    void setCastPort(int v);
    void setSelectedNetworkIface(const QString& v);
    void setNetworkThreadCount(int v);

    void setRecordingFolder(const QString& v);
    void setRecordingQuality(const QString& v);
    void setAutoRecord(bool v);

    void setRtmpUrl(const QString& v);
    void setStreamKey(const QString& v);
    void setEncoderPreference(const QString& v);
    void setStreamBitrateKbps(int v);
    void setVirtualCameraEnabled(bool v);

    void setShowOverlay(bool v);
    void setDarkMode(bool v);
    void setAlwaysOnTop(bool v);
    void setStartWithWindows(bool v);
    void setMinimiseToTray(bool v);
    void setShowOverlayOnStart(bool v);
    void setLanguage(const QString& v);

    void setGpuFrameBufferCount(int v);
    void setLogLevel(const QString& v);

    void setTelemetryEnabled(bool v);

    void setLicenseKey(const QString& v);
    void setGpuName(const QString& v);
    void setOsVersion(const QString& v);

    // Read-only getters for license + system info + disk warn
    int     diskWarnGb()     const { return m_diskWarnGb; }
    QString licenseKey()     const { return m_licenseKey; }
    QString edition()        const { return m_edition; }
    void    setEdition(const QString& v) { if(m_edition!=v){m_edition=v;emit licenseChanged();} }
    QString licenseStatus()  const { return m_licenseStatus; }
    void    setLicenseStatus(const QString& v) { if(m_licenseStatus!=v){m_licenseStatus=v;emit licenseChanged();} }
    QString appVersion()     const;
    QString buildDate()      const;
    QString gpuName()        const { return m_gpuName; }
    QString osVersion()      const { return m_osVersion; }

signals:
    void identityChanged();
    void videoChanged();
    void audioChanged();
    void networkChanged();
    void recordingChanged();
    void streamingChanged();
    void uiChanged();
    void licenseChanged();
    void systemInfoChanged();
    Q_SIGNAL void never();  // placeholder for read-only const properties
    void advancedChanged();
    void privacyChanged();

private:
    // Identity
    QString m_displayName{"AuraCastPro"};
    bool    m_firstRunCompleted{false};

    // Video
    int     m_maxWidth{1920};
    int     m_maxHeight{1080};
    int     m_maxFps{60};
    int     m_maxResolutionIndex{1};   // 0=720p 1=1080p 2=1440p 3=4K
    int     m_fpsCapIndex{1};          // 0=30 1=60 2=90 3=120
    bool    m_hdrEnabled{false};
    bool    m_hardwareDecodeEnabled{true};
    QString m_preferredCodec{"h265"};

    // Audio
    bool    m_audioEnabled{true};
    bool    m_micEnabled{false};
    bool    m_virtualAudioEnabled{false};
    QString m_outputDeviceId{};
    double  m_deviceVolume{1.0};
    double  m_micVolume{0.7};
    int     m_avSyncOffsetMs{0};

    // Network
    int     m_maxBitrateKbps{20000};
    bool    m_fecEnabled{true};
    int     m_jitterBufferMs{50};
    int     m_airplayPort{7236};
    int     m_castPort{8009};
    QString m_selectedNetworkIface{};
    int     m_networkThreadCount{0};   // 0 = auto

    // Recording
    QString m_recordingFolder{};       // set to Videos/AuraCastPro in ctor
    QString m_recordingQuality{"source"};
    bool    m_autoRecord{false};

    // Streaming
    QString m_rtmpUrl{};
    QString m_streamKey{};             // stored encrypted via SecurityVault
    QString m_encoderPreference{"auto"};
    int     m_streamBitrateKbps{6000};
    bool    m_virtualCameraEnabled{false};

    // UI
    bool    m_showOverlay{false};
    bool    m_darkMode{true};
    bool    m_alwaysOnTop{false};
    bool    m_startWithWindows{false};
    bool    m_minimiseToTray{true};
    bool    m_showOverlayOnStart{false};
    QString m_language{"en"};
    // Alias members
    int     m_diskWarnGb{5};
    QString m_licenseKey;
    QString m_edition{"Free"};
    QString m_licenseStatus{"Unlicensed"};
    QString m_gpuName;
    QString m_osVersion;

    // Advanced
    int     m_gpuFrameBufferCount{4};
    QString m_logLevel{"info"};

    // Privacy
    bool    m_telemetryEnabled{false};

    std::string settingsFilePath() const;

    SecurityVault* m_vault{nullptr};  // non-owning; set from main() after vault init
};

} // namespace aura
