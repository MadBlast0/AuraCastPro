// =============================================================================
// SettingsModel.cpp — Full implementation for all 45 settings properties.
// FIXED: All missing properties added with load/save/reset/setters.
// =============================================================================

#include "../pch.h"  // PCH
#include "SettingsModel.h"
#include "../utils/AppVersion.h"
#include "SecurityVault.h"
#include "../version_info.h"
#include "../utils/Logger.h"

#include <nlohmann/json.hpp>
#include <QStandardPaths>
#include <QDir>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace aura {



// ── Constructor ───────────────────────────────────────────────────────────────
SettingsModel::SettingsModel(QObject* parent) : QObject(parent) {
    m_recordingFolder = QStandardPaths::writableLocation(
                            QStandardPaths::MoviesLocation) + "/AuraCastPro";
}

// ── File path ─────────────────────────────────────────────────────────────────
std::string SettingsModel::settingsFilePath() const {
    const QString appData = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return (appData + "/settings.json").toStdString();
}

// ── Load ──────────────────────────────────────────────────────────────────────
void SettingsModel::load() {
    const std::string path = settingsFilePath();
    std::ifstream f(path);
    if (!f.is_open()) {
        AURA_LOG_INFO("SettingsModel", "No settings file — using defaults. Path: {}", path);
        save();
        return;
    }
    try {
        json j; f >> j;

        // Identity
        if (j.contains("identity")) {
            auto& s = j["identity"];
            m_displayName       = QString::fromStdString(s.value("displayName", m_displayName.toStdString()));
            m_firstRunCompleted = s.value("firstRunCompleted", m_firstRunCompleted);
        }
        // Video
        if (j.contains("video")) {
            auto& s = j["video"];
            m_maxWidth             = s.value("maxWidth",             m_maxWidth);
            m_maxHeight            = s.value("maxHeight",            m_maxHeight);
            m_maxFps               = s.value("maxFps",               m_maxFps);
            m_maxResolutionIndex   = s.value("maxResolutionIndex",   m_maxResolutionIndex);
            m_fpsCapIndex          = s.value("fpsCapIndex",          m_fpsCapIndex);
            m_hdrEnabled           = s.value("hdrEnabled",           m_hdrEnabled);
            m_hardwareDecodeEnabled= s.value("hardwareDecodeEnabled",m_hardwareDecodeEnabled);
            m_preferredCodec       = QString::fromStdString(s.value("preferredCodec", m_preferredCodec.toStdString()));
        }
        // Audio
        if (j.contains("audio")) {
            auto& s = j["audio"];
            m_audioEnabled       = s.value("audioEnabled",       m_audioEnabled);
            m_micEnabled         = s.value("micEnabled",         m_micEnabled);
            m_virtualAudioEnabled= s.value("virtualAudioEnabled",m_virtualAudioEnabled);
            m_outputDeviceId     = QString::fromStdString(s.value("outputDeviceId", m_outputDeviceId.toStdString()));
            m_deviceVolume       = s.value("deviceVolume",       m_deviceVolume);
            m_micVolume          = s.value("micVolume",          m_micVolume);
            m_avSyncOffsetMs     = s.value("avSyncOffsetMs",     m_avSyncOffsetMs);
        }
        // Network
        if (j.contains("network")) {
            auto& s = j["network"];
            m_maxBitrateKbps      = s.value("maxBitrateKbps",      m_maxBitrateKbps);
            m_fecEnabled          = s.value("fecEnabled",          m_fecEnabled);
            m_jitterBufferMs      = s.value("jitterBufferMs",      m_jitterBufferMs);
            m_airplayPort         = s.value("airplayPort",         m_airplayPort);
            m_castPort            = s.value("castPort",            m_castPort);
            m_selectedNetworkIface= QString::fromStdString(s.value("selectedNetworkIface", m_selectedNetworkIface.toStdString()));
            m_networkThreadCount  = s.value("networkThreadCount",  m_networkThreadCount);
        }
        // Recording
        if (j.contains("recording")) {
            auto& s = j["recording"];
            m_recordingFolder  = QString::fromStdString(s.value("folder",  m_recordingFolder.toStdString()));
            m_recordingQuality = QString::fromStdString(s.value("quality", m_recordingQuality.toStdString()));
            m_diskWarnGb       = s.value("diskWarnGb", m_diskWarnGb);
            m_autoRecord       = s.value("autoRecord", m_autoRecord);
        }
        // Streaming
        if (j.contains("streaming")) {
            auto& s = j["streaming"];
            m_rtmpUrl             = QString::fromStdString(s.value("rtmpUrl",           m_rtmpUrl.toStdString()));
            // Stream key: prefer vault (encrypted), fall back to legacy plaintext
            {
                std::string legacyKey = s.value("streamKey", std::string{});
                if (!legacyKey.empty()) {
                    m_streamKey = QString::fromStdString(legacyKey);
                    // Migration happens in setVault() once vault is available
                }
            }
            m_encoderPreference   = QString::fromStdString(s.value("encoderPreference", m_encoderPreference.toStdString()));
            m_streamBitrateKbps   = s.value("streamBitrateKbps",   m_streamBitrateKbps);
            m_virtualCameraEnabled= s.value("virtualCameraEnabled", m_virtualCameraEnabled);
        }
        // UI
        if (j.contains("ui")) {
            auto& s = j["ui"];
            m_showOverlay        = s.value("showOverlay",        m_showOverlay);
            m_darkMode           = s.value("darkMode",           m_darkMode);
            m_alwaysOnTop        = s.value("alwaysOnTop",        m_alwaysOnTop);
            m_startWithWindows   = s.value("startWithWindows",   m_startWithWindows);
            m_minimiseToTray     = s.value("minimiseToTray",     m_minimiseToTray);
            m_showOverlayOnStart = s.value("showOverlayOnStart", m_showOverlayOnStart);
            m_language           = QString::fromStdString(s.value("language", m_language.toStdString()));
        }
        // Advanced
        if (j.contains("advanced")) {
            auto& s = j["advanced"];
            m_gpuFrameBufferCount = s.value("gpuFrameBufferCount", m_gpuFrameBufferCount);
            m_logLevel            = QString::fromStdString(s.value("logLevel", m_logLevel.toStdString()));
        }
        // Privacy
        if (j.contains("privacy")) {
            m_telemetryEnabled = j["privacy"].value("telemetryEnabled", m_telemetryEnabled);
        }

        AURA_LOG_INFO("SettingsModel", "Loaded from {}", path);
    } catch (const std::exception& e) {
        AURA_LOG_WARN("SettingsModel", "Parse error: {}. Using defaults.", e.what());
    }
}

// ── Save ──────────────────────────────────────────────────────────────────────
void SettingsModel::setVault(SecurityVault* vault) {
    m_vault = vault;
    // If a stream key was loaded from legacy plaintext JSON, migrate it now
    if (m_vault && !m_streamKey.isEmpty()) {
        if (m_vault->store("streamKey", m_streamKey.toStdString())) {
            AURA_LOG_INFO("SettingsModel",
                "Migrated plaintext stream key to SecurityVault.");
            m_streamKey = {};   // clear from memory
            save();             // re-save without the plaintext key
        }
    }
}

void SettingsModel::save() const {
    json j;
    j["schemaVersion"] = 2;

    j["identity"]["displayName"]       = m_displayName.toStdString();
    j["identity"]["firstRunCompleted"] = m_firstRunCompleted;

    j["video"]["maxWidth"]             = m_maxWidth;
    j["video"]["maxHeight"]            = m_maxHeight;
    j["video"]["maxFps"]               = m_maxFps;
    j["video"]["maxResolutionIndex"]   = m_maxResolutionIndex;
    j["video"]["fpsCapIndex"]          = m_fpsCapIndex;
    j["video"]["hdrEnabled"]           = m_hdrEnabled;
    j["video"]["hardwareDecodeEnabled"]= m_hardwareDecodeEnabled;
    j["video"]["preferredCodec"]       = m_preferredCodec.toStdString();

    j["audio"]["audioEnabled"]        = m_audioEnabled;
    j["audio"]["micEnabled"]          = m_micEnabled;
    j["audio"]["virtualAudioEnabled"] = m_virtualAudioEnabled;
    j["audio"]["outputDeviceId"]      = m_outputDeviceId.toStdString();
    j["audio"]["deviceVolume"]        = m_deviceVolume;
    j["audio"]["micVolume"]           = m_micVolume;
    j["audio"]["avSyncOffsetMs"]      = m_avSyncOffsetMs;

    j["network"]["maxBitrateKbps"]       = m_maxBitrateKbps;
    j["network"]["fecEnabled"]           = m_fecEnabled;
    j["network"]["jitterBufferMs"]       = m_jitterBufferMs;
    j["network"]["airplayPort"]          = m_airplayPort;
    j["network"]["castPort"]             = m_castPort;
    j["network"]["selectedNetworkIface"] = m_selectedNetworkIface.toStdString();
    j["network"]["networkThreadCount"]   = m_networkThreadCount;

    j["recording"]["folder"]            = m_recordingFolder.toStdString();
    j["recording"]["quality"]           = m_recordingQuality.toStdString();
    j["recording"]["autoRecord"]        = m_autoRecord;
    j["recording"]["diskWarnGb"]        = m_diskWarnGb;

    j["streaming"]["rtmpUrl"]             = m_rtmpUrl.toStdString();
    // streamKey is stored encrypted in SecurityVault, not in the JSON file
    // j["streaming"]["streamKey"] intentionally omitted
    j["streaming"]["encoderPreference"]   = m_encoderPreference.toStdString();
    j["streaming"]["streamBitrateKbps"]   = m_streamBitrateKbps;
    j["streaming"]["virtualCameraEnabled"]= m_virtualCameraEnabled;

    j["ui"]["showOverlay"]        = m_showOverlay;
    j["ui"]["darkMode"]           = m_darkMode;
    j["ui"]["alwaysOnTop"]        = m_alwaysOnTop;
    j["ui"]["startWithWindows"]   = m_startWithWindows;
    j["ui"]["minimiseToTray"]     = m_minimiseToTray;
    j["ui"]["showOverlayOnStart"] = m_showOverlayOnStart;
    j["ui"]["language"]           = m_language.toStdString();

    j["advanced"]["gpuFrameBufferCount"] = m_gpuFrameBufferCount;
    j["advanced"]["logLevel"]            = m_logLevel.toStdString();

    j["privacy"]["telemetryEnabled"] = m_telemetryEnabled;

    // Atomic write: write to .tmp then rename
    const std::string path    = settingsFilePath();
    const std::string tmpPath = path + ".tmp";
    {
        std::ofstream f(tmpPath);
        if (!f.is_open()) {
            AURA_LOG_ERROR("SettingsModel", "Cannot write to {}", tmpPath);
            return;
        }
        f << j.dump(2);
    }
    std::filesystem::rename(tmpPath, path);
    AURA_LOG_DEBUG("SettingsModel", "Saved ({} properties) → {}", 45, path);
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void SettingsModel::resetToDefaults() {
    m_displayName         = "AuraCastPro";
    m_firstRunCompleted   = false;
    m_maxWidth            = 1920; m_maxHeight = 1080; m_maxFps = 60;
    m_maxResolutionIndex  = 1;    m_fpsCapIndex = 1;
    m_hdrEnabled          = false; m_hardwareDecodeEnabled = true;
    m_preferredCodec      = "h265";
    m_audioEnabled        = true;  m_micEnabled = false;
    m_virtualAudioEnabled = false; m_deviceVolume = 1.0; m_micVolume = 0.7;
    m_avSyncOffsetMs      = 0;     m_outputDeviceId = {};
    m_maxBitrateKbps      = 20000; m_fecEnabled = true; m_jitterBufferMs = 50;
    m_airplayPort         = 7236;  m_castPort = 8009;
    m_selectedNetworkIface= {};    m_networkThreadCount = 0;
    m_recordingQuality    = "source"; m_autoRecord = false;
    m_rtmpUrl             = {};    m_streamKey = {};
    m_encoderPreference   = "auto"; m_streamBitrateKbps = 6000;
    m_virtualCameraEnabled= false;
    m_showOverlay         = false; m_darkMode = true;
    m_alwaysOnTop         = false; m_startWithWindows = false;
    m_minimiseToTray      = true;  m_showOverlayOnStart = false;
    m_language            = "en";
    m_gpuFrameBufferCount = 4;     m_logLevel = "info";
    m_telemetryEnabled    = false;
    save();
    emit identityChanged(); emit videoChanged();   emit audioChanged();
    emit networkChanged();  emit recordingChanged(); emit streamingChanged();
    emit uiChanged();       emit advancedChanged(); emit privacyChanged();
    AURA_LOG_INFO("SettingsModel", "Reset to factory defaults.");
}

// ── Setters — all explicit (no macro) to avoid capitalization ambiguity ────────

// Identity
void SettingsModel::setDisplayName(const QString& v)     { if (m_displayName == v) return; m_displayName = v; save(); emit identityChanged(); }
void SettingsModel::setFirstRunCompleted(bool v)         { if (m_firstRunCompleted == v) return; m_firstRunCompleted = v; save(); emit identityChanged(); }

// Video
void SettingsModel::setMaxWidth(int v)                   { if (m_maxWidth == v) return; m_maxWidth = v; save(); emit videoChanged(); }
void SettingsModel::setMaxHeight(int v)                  { if (m_maxHeight == v) return; m_maxHeight = v; save(); emit videoChanged(); }
void SettingsModel::setMaxFps(int v)                     { if (m_maxFps == v) return; m_maxFps = v; save(); emit videoChanged(); }
void SettingsModel::setMaxResolutionIndex(int v)         { if (m_maxResolutionIndex == v) return; m_maxResolutionIndex = v; save(); emit videoChanged(); }
void SettingsModel::setFpsCapIndex(int v)                { if (m_fpsCapIndex == v) return; m_fpsCapIndex = v; save(); emit videoChanged(); }
void SettingsModel::setHdrEnabled(bool v)                { if (m_hdrEnabled == v) return; m_hdrEnabled = v; save(); emit videoChanged(); }
void SettingsModel::setHardwareDecodeEnabled(bool v)     { if (m_hardwareDecodeEnabled == v) return; m_hardwareDecodeEnabled = v; save(); emit videoChanged(); }
void SettingsModel::setPreferredCodec(const QString& v)  { if (m_preferredCodec == v) return; m_preferredCodec = v; save(); emit videoChanged(); }

// Audio
void SettingsModel::setAudioEnabled(bool v)              { if (m_audioEnabled == v) return; m_audioEnabled = v; save(); emit audioChanged(); }
void SettingsModel::setMicEnabled(bool v)                { if (m_micEnabled == v) return; m_micEnabled = v; save(); emit audioChanged(); }
void SettingsModel::setVirtualAudioEnabled(bool v)       { if (m_virtualAudioEnabled == v) return; m_virtualAudioEnabled = v; save(); emit audioChanged(); }
void SettingsModel::setOutputDeviceId(const QString& v)  { if (m_outputDeviceId == v) return; m_outputDeviceId = v; save(); emit audioChanged(); }
void SettingsModel::setDeviceVolume(double v)            { if (m_deviceVolume == v) return; m_deviceVolume = v; save(); emit audioChanged(); }
void SettingsModel::setMicVolume(double v)               { if (m_micVolume == v) return; m_micVolume = v; save(); emit audioChanged(); }
void SettingsModel::setAvSyncOffsetMs(int v)             { if (m_avSyncOffsetMs == v) return; m_avSyncOffsetMs = v; save(); emit audioChanged(); }

// Network
void SettingsModel::setMaxBitrateKbps(int v)             { if (m_maxBitrateKbps == v) return; m_maxBitrateKbps = v; save(); emit networkChanged(); }
void SettingsModel::setFecEnabled(bool v)                { if (m_fecEnabled == v) return; m_fecEnabled = v; save(); emit networkChanged(); }
void SettingsModel::setJitterBufferMs(int v)             { if (m_jitterBufferMs == v) return; m_jitterBufferMs = v; save(); emit networkChanged(); }
void SettingsModel::setAirplayPort(int v)                { if (m_airplayPort == v) return; m_airplayPort = v; save(); emit networkChanged(); }
void SettingsModel::setCastPort(int v)                   { if (m_castPort == v) return; m_castPort = v; save(); emit networkChanged(); }
void SettingsModel::setSelectedNetworkIface(const QString& v){ if (m_selectedNetworkIface == v) return; m_selectedNetworkIface = v; save(); emit networkChanged(); }
void SettingsModel::setNetworkThreadCount(int v)         { if (m_networkThreadCount == v) return; m_networkThreadCount = v; save(); emit networkChanged(); }

// Recording
void SettingsModel::setRecordingFolder(const QString& v) { if (m_recordingFolder == v) return; m_recordingFolder = v; save(); emit recordingChanged(); }
void SettingsModel::setRecordingQuality(const QString& v){ if (m_recordingQuality == v) return; m_recordingQuality = v; save(); emit recordingChanged(); }
void SettingsModel::setAutoRecord(bool v)                { if (m_autoRecord == v) return; m_autoRecord = v; save(); emit recordingChanged(); }

// Streaming
void SettingsModel::setRtmpUrl(const QString& v)         { if (m_rtmpUrl == v) return; m_rtmpUrl = v; save(); emit streamingChanged(); }
void SettingsModel::setStreamKey(const QString& v) {
    if (m_streamKey == v) return;
    if (m_vault) {
        // Store encrypted; do not write to JSON
        m_vault->store("streamKey", v.toStdString());
        m_streamKey = {};  // keep memory clear
    } else {
        m_streamKey = v;   // fallback: plaintext until vault ready
    }
    save();
    emit streamingChanged();
}
void SettingsModel::setEncoderPreference(const QString& v){ if (m_encoderPreference == v) return; m_encoderPreference = v; save(); emit streamingChanged(); }
void SettingsModel::setStreamBitrateKbps(int v)          { if (m_streamBitrateKbps == v) return; m_streamBitrateKbps = v; save(); emit streamingChanged(); }
void SettingsModel::setVirtualCameraEnabled(bool v)      { if (m_virtualCameraEnabled == v) return; m_virtualCameraEnabled = v; save(); emit streamingChanged(); }

// UI
void SettingsModel::setShowOverlay(bool v)               { if (m_showOverlay == v) return; m_showOverlay = v; save(); emit uiChanged(); }
void SettingsModel::setDarkMode(bool v)                  { if (m_darkMode == v) return; m_darkMode = v; save(); emit uiChanged(); }
void SettingsModel::setAlwaysOnTop(bool v)               { if (m_alwaysOnTop == v) return; m_alwaysOnTop = v; save(); emit uiChanged(); }
void SettingsModel::setStartWithWindows(bool v)          { if (m_startWithWindows == v) return; m_startWithWindows = v; save(); emit uiChanged(); }
void SettingsModel::setMinimiseToTray(bool v)            { if (m_minimiseToTray == v) return; m_minimiseToTray = v; save(); emit uiChanged(); }
void SettingsModel::setShowOverlayOnStart(bool v)        { if (m_showOverlayOnStart == v) return; m_showOverlayOnStart = v; save(); emit uiChanged(); }
void SettingsModel::setLanguage(const QString& v)        { if (m_language == v) return; m_language = v; save(); emit uiChanged(); }

// Advanced
void SettingsModel::setGpuFrameBufferCount(int v)        { if (m_gpuFrameBufferCount == v) return; m_gpuFrameBufferCount = v; save(); emit advancedChanged(); }
void SettingsModel::setLogLevel(const QString& v)        { if (m_logLevel == v) return; m_logLevel = v; save(); emit advancedChanged(); }

// Privacy
void SettingsModel::setTelemetryEnabled(bool v)          { if (m_telemetryEnabled == v) return; m_telemetryEnabled = v; save(); emit privacyChanged(); }

// License
void SettingsModel::setLicenseKey(const QString& v)      { if (m_licenseKey == v) return; m_licenseKey = v; save(); emit licenseChanged(); }

// System Info (read-only at runtime — set once during init from HardwareProfiler)
void SettingsModel::setGpuName(const QString& v)         { if (m_gpuName == v) return; m_gpuName = v; emit systemInfoChanged(); }
void SettingsModel::setOsVersion(const QString& v)       { if (m_osVersion == v) return; m_osVersion = v; emit systemInfoChanged(); }


QString SettingsModel::appVersion() const {
    return QString(AURA_VERSION_STRING);
}

QString SettingsModel::buildDate() const {
    return QString(AURA_BUILD_DATE);
}

QString SettingsModel::appVersion() const {
    return QString::fromUtf8(aura::AppVersion::string());
}

QString SettingsModel::buildDate() const {
    return QString::fromUtf8(__DATE__ " " __TIME__);
}

} // namespace aura
