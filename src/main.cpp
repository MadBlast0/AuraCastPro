// =============================================================================
// main.cpp -- AuraCastPro complete startup + shutdown sequence
// =============================================================================
#include "pch.h"  // PCH
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <mfapi.h>    // MFStartup, MFShutdown, MF_VERSION
#include <mfidl.h>    // IMFMediaSource (used by VideoDecoder)
#include <objbase.h>  // CoInitializeEx, CoUninitialize
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>


#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "ole32.lib")

#include <QApplication>
#include <QDir>
#include <QNetworkProxyFactory>
#include <QPixmap>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>

#include "version_info.h"  // Task 024: auto-generated version header


// -- Utils ---------------------------------------------------------------------
#include "utils/CrashReporter.h"
#include "utils/DiagnosticsExporter.h"
#include "utils/EncryptHelper.h"
#include "utils/HardwareProfiler.h"
#include "utils/Logger.h"
#include "utils/NetworkTools.h"
#include "utils/OSVersionHelper.h"
#include "utils/PerformanceTimer.h"
#include "utils/WindowsEventLog.h"  // Task 159


// -- Display -------------------------------------------------------------------
#include "display/DX12Manager.h"
#include "display/MirrorWindow.h"

// -- Engine --------------------------------------------------------------------
#include "engine/ADBBridge.h"
#include "engine/AV1Demuxer.h"
#include "engine/AirPlay2Host.h"
#include "engine/BitratePID.h"
#include "engine/CastV2Host.h"
#include "engine/FECRecovery.h"
#include "engine/FrameTimingQueue.h"
#include "engine/H265Demuxer.h"
#include "engine/NALParser.h"
#include "engine/NetworkPredictor.h"
#include "engine/PacketReorderCache.h"
#include "engine/ProtocolHandshake.h"
#include "engine/ReceiverSocket.h"
#include "engine/ReconnectManager.h"
#include "engine/UDPServerThreadPool.h"
#include "engine/VideoDecoder.h"


// -- Discovery -----------------------------------------------------------------
#include "discovery/BonjourAdapter.h"
#include "discovery/DeviceAdvertiser.h"
#include "discovery/MDNSService.h"


// -- Integration ---------------------------------------------------------------
#include "integration/AudioLoopback.h"
#include "integration/AudioMixer.h"
#include "integration/DiskSpaceMonitor.h"  // Task: low-disk auto-stop recording
#include "integration/StreamRecorder.h"
#include "integration/VCamBridge.h"
#include "integration/VirtualAudioDriver.h"


// -- Streaming -----------------------------------------------------------------
#include "streaming/HardwareEncoder.h"
#include "streaming/NVENCWrapper.h"
#include "streaming/RTMPOutput.h"
#include "streaming/StreamBroadcaster.h"

// -- Input ---------------------------------------------------------------------
#include "engine/USBHotplug.h"  // Task 062
#include "input/AndroidControlBridge.h"
#include "input/ClipboardBridge.h"  // Task 150
#include "input/KeyboardToInput.h"
#include "input/MouseToTouch.h"
#include "manager/WindowsServices.h"  // Task 178


// -- Manager -------------------------------------------------------------------
#include "manager/DeviceManager.h"
#include "manager/ErrorDialog.h"   // Task 177
#include "manager/GlobalHotkey.h"  // Task 175
#include "manager/HubModel.h"
#include "manager/HubWindow.h"
#include "manager/NetworkStatsModel.h"
#include "manager/PerformanceOverlay.h"
#include "manager/SecurityVault.h"
#include "manager/SettingsModel.h"
#include "manager/ToastNotification.h"  // Task 176


// -- Licensing -----------------------------------------------------------------
#include "licensing/FeatureGate.h"
#include "licensing/LicenseManager.h"
#include "licensing/LicenseValidator.h"


// -- Plugins -------------------------------------------------------------------
#include "plugins/PluginManager.h"

// -- Cloud ---------------------------------------------------------------------
#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "../cloud/LicenseClient.h"
#include "../cloud/TelemetryClient.h"
#include "../cloud/UpdateService.h"


#ifndef AURA_VERSION_STRING
#define AURA_VERSION_STRING "1.0.0"
#endif

// =============================================================================
// Global subsystem instances (owned here; shut down in reverse order)
// =============================================================================
// NOTE: ReceiverSocket and UDPServerThreadPool share ownership of the socket
// via a shared_ptr.  The global unique_ptr below holds a non-owning alias so
// the shutdown helper can call ->shutdown() without double-free.
static std::unique_ptr<aura::SettingsModel> g_settings;
static std::unique_ptr<aura::SecurityVault> g_vault;
static std::unique_ptr<aura::LicenseManager> g_license;
static std::unique_ptr<aura::LicenseValidator> g_licenseValidator;
static std::unique_ptr<aura::FeatureGate> g_featureGate;
static std::unique_ptr<aura::DX12Manager> g_dx12;
static std::unique_ptr<aura::AudioLoopback> g_audioLoopback;
static std::unique_ptr<aura::AudioMixer> g_audioMixer;
static std::unique_ptr<aura::VirtualAudioDriver> g_virtualAudio;
static std::unique_ptr<aura::VCamBridge> g_vcam;
static std::unique_ptr<ReconnectManager> g_reconnect;
static std::unique_ptr<aura::StreamRecorder> g_recorder;
static std::unique_ptr<aura::DiskSpaceMonitor> g_diskMonitor;  // low-disk auto-stop
static std::unique_ptr<aura::RTMPOutput> g_rtmp;
static std::unique_ptr<aura::HardwareEncoder> g_encoder;
static std::unique_ptr<aura::NVENCWrapper> g_nvenc;
// Shared ownership -- UDPServerThreadPool and the receive callback both hold
// a shared_ptr so neither dangling-ref the other.
static std::shared_ptr<aura::ReceiverSocket> g_sharedSocket;
static std::unique_ptr<aura::UDPServerThreadPool> g_udpPool;
static std::unique_ptr<aura::PacketReorderCache> g_reorderCache;
static std::unique_ptr<aura::FECRecovery> g_fec;
static std::unique_ptr<aura::NALParser> g_nalParser;
static std::unique_ptr<aura::H265Demuxer> g_h265Demuxer;
static std::unique_ptr<aura::AV1Demuxer> g_av1Demuxer;
static std::unique_ptr<aura::VideoDecoder> g_videoDecoder;
static std::unique_ptr<aura::BitratePID> g_bitratePID;
static std::unique_ptr<aura::NetworkPredictor> g_networkPredictor;
static std::unique_ptr<aura::FrameTimingQueue> g_frameTiming;
static std::unique_ptr<aura::ProtocolHandshake> g_handshake;
static std::unique_ptr<aura::AirPlay2Host> g_airplay;
static std::unique_ptr<aura::CastV2Host> g_cast;
static std::unique_ptr<aura::ADBBridge> g_adb;
static std::unique_ptr<aura::BonjourAdapter> g_bonjour;
static std::unique_ptr<aura::MDNSService> g_mdns;
static std::unique_ptr<aura::DeviceAdvertiser> g_advertiser;
static std::unique_ptr<aura::MouseToTouch> g_mouseToTouch;
static std::unique_ptr<aura::KeyboardToInput> g_keyboardInput;
static std::unique_ptr<aura::AndroidControlBridge> g_androidControl;
static std::unique_ptr<aura::ClipboardBridge> g_clipboard;  // Task 150
static std::unique_ptr<aura::USBHotplug> g_usbHotplug;      // Task 062
static std::unique_ptr<aura::MirrorWindow> g_mirrorWindow;
static std::unique_ptr<aura::DeviceManager> g_deviceManager;
static std::unique_ptr<aura::NetworkStatsModel> g_networkStats;
static std::unique_ptr<aura::PerformanceOverlay> g_overlay;
static std::unique_ptr<aura::PluginManager> g_plugins;
static std::unique_ptr<aura::StreamBroadcaster> g_broadcaster;
static std::unique_ptr<aura::HubWindow> g_hubWindow;
static std::unique_ptr<aura::UpdateService> g_updater;
static std::unique_ptr<aura::LicenseClient> g_licenseClient;
static std::unique_ptr<aura::TelemetryClient> g_telemetry;

// =============================================================================
// Helper: ensure AppData directories exist
// =============================================================================
static void ensureDirectories() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base + "/logs");
    QDir().mkpath(base + "/plugins");
    QDir().mkpath(base + "/recordings");
    QDir().mkpath(base + "/cache");
    QDir().mkpath(base + "/crashes");
}

// =============================================================================
// SAFE_INIT macros -- log failure but keep running for non-critical subsystems
// =============================================================================
#define SAFE_INIT(name, expr)                                         \
    try {                                                             \
        expr;                                                         \
        AURA_LOG_INFO("main", "{} initialised.", name);               \
    } catch (const std::exception& e) {                               \
        AURA_LOG_ERROR("main", "{} init failed: {}", name, e.what()); \
    }

#define SAFE_INIT_CRITICAL(name, expr)                                              \
    try {                                                                           \
        expr;                                                                       \
        AURA_LOG_INFO("main", "{} initialised.", name);                             \
    } catch (const std::exception& e) {                                             \
        AURA_LOG_CRITICAL("main", "{} CRITICAL FAILURE: {}", name, e.what());       \
        MessageBoxA(nullptr, (std::string(name) + " failed:\n" + e.what()).c_str(), \
                    "AuraCastPro -- Fatal Error", MB_ICONERROR);                    \
        return 1;                                                                   \
    }

// =============================================================================
// Shutdown -- called once before process exit, in reverse construction order
// =============================================================================
static void shutdownAll() {
    auto& log = *aura::Logger::get();
    log.info("[main] Beginning shutdown sequence...");

    auto safe = [&](const char* name, auto fn) {
        try {
            fn();
            log.info("[main] {} shutdown OK.", name);
        } catch (const std::exception& e) {
            log.error("[main] {} shutdown error: {}", name, e.what());
        }
    };

    safe("GlobalHotkey", [&] { aura::GlobalHotkey::instance().unregisterAll(); });
    safe("ToastNotification", [&] { aura::ToastNotification::shutdown(); });
    safe("AirPlay2Host", [&] {
        if (g_airplay) {
            g_airplay->shutdown();
            g_airplay.reset();
        }
    });
    safe("CastV2Host", [&] {
        if (g_cast) {
            g_cast->shutdown();
            g_cast.reset();
        }
    });
    safe("ADBBridge", [&] {
        if (g_adb) {
            g_adb->shutdown();
            g_adb.reset();
        }
    });
    safe("DeviceAdvertiser", [&] {
        if (g_advertiser) {
            g_advertiser->shutdown();
            g_advertiser.reset();
        }
    });
    safe("MDNSService", [&] {
        if (g_mdns) {
            g_mdns->shutdown();
            g_mdns.reset();
        }
    });
    safe("BonjourAdapter", [&] {
        if (g_bonjour) {
            g_bonjour->shutdown();
            g_bonjour.reset();
        }
    });
    safe("HubWindow", [&] { g_hubWindow.reset(); });
    safe("UpdateService", [&] {
        if (g_updater) {
            g_updater->shutdown();
            g_updater.reset();
        }
    });
    safe("TelemetryClient", [&] {
        if (g_telemetry) {
            g_telemetry->shutdown();
            g_telemetry.reset();
        }
    });
    safe("LicenseClient", [&] {
        if (g_licenseClient) {
            g_licenseClient->shutdown();
            g_licenseClient.reset();
        }
    });
    safe("PluginManager", [&] {
        if (g_plugins) {
            g_plugins->shutdown();
            g_plugins.reset();
        }
    });
    safe("StreamBroadcaster", [&] {
        if (g_broadcaster) {
            g_broadcaster->shutdown();
            g_broadcaster.reset();
        }
    });
    safe("RTMPOutput", [&] {
        if (g_rtmp) {
            g_rtmp->shutdown();
            g_rtmp.reset();
        }
    });
    safe("DiskSpaceMonitor", [&] {
        if (g_diskMonitor) {
            g_diskMonitor->shutdown();
            g_diskMonitor.reset();
        }
    });
    safe("StreamRecorder", [&] {
        if (g_recorder) {
            g_recorder->shutdown();
            g_recorder.reset();
        }
    });
    safe("VCamBridge", [&] {
        if (g_vcam) {
            g_vcam->shutdown();
            g_vcam.reset();
        }
    });
    safe("AndroidControl", [&] {
        if (g_androidControl) {
            g_androidControl->shutdown();
            g_androidControl.reset();
        }
    });
    safe("KeyboardInput", [&] {
        if (g_keyboardInput) {
            g_keyboardInput->shutdown();
            g_keyboardInput.reset();
        }
    });
    safe("ClipboardBridge", [&] {
        if (g_clipboard) {
            g_clipboard->stopMonitoring();
            g_clipboard.reset();
        }
    });
    safe("USBHotplug", [&] {
        if (g_usbHotplug) {
            g_usbHotplug->stop();
            g_usbHotplug.reset();
        }
    });
    safe("MouseToTouch", [&] {
        if (g_mouseToTouch) {
            g_mouseToTouch->shutdown();
            g_mouseToTouch.reset();
        }
    });
    // Stop IO threads before closing the socket
    safe("ReconnectManager", [&] {
        if (g_reconnect) {
            g_reconnect->stop();
            g_reconnect.reset();
        }
    });
    safe("UDPPool", [&] {
        if (g_udpPool) {
            g_udpPool->stop();
            g_udpPool.reset();
        }
    });
    safe("ReceiverSocket", [&] { g_sharedSocket.reset(); });
    safe("VideoDecoder", [&] {
        if (g_videoDecoder) {
            g_videoDecoder->shutdown();
            g_videoDecoder.reset();
        }
    });
    safe("MirrorWindow", [&] {
        if (g_mirrorWindow) {
            g_mirrorWindow->shutdown();
            g_mirrorWindow.reset();
        }
    });
    safe("DX12Manager", [&] {
        if (g_dx12) {
            g_dx12->shutdown();
            g_dx12.reset();
        }
    });
    safe("VirtualAudio", [&] {
        if (g_virtualAudio) {
            g_virtualAudio->shutdown();
            g_virtualAudio.reset();
        }
    });
    safe("AudioMixer", [&] {
        if (g_audioMixer) {
            g_audioMixer->shutdown();
            g_audioMixer.reset();
        }
    });
    safe("AudioLoopback", [&] {
        if (g_audioLoopback) {
            g_audioLoopback->shutdown();
            g_audioLoopback.reset();
        }
    });
    safe("FeatureGate", [&] {
        if (g_featureGate) {
            g_featureGate->shutdown();
            g_featureGate.reset();
        }
    });
    safe("LicenseManager", [&] {
        if (g_license) {
            g_license->shutdown();
            g_license.reset();
        }
    });
    safe("SecurityVault", [&] {
        if (g_vault) {
            g_vault->shutdown();
            g_vault.reset();
        }
    });
    safe("DeviceManager", [&] { g_deviceManager.reset(); });
    safe("Settings", [&] { g_settings.reset(); });

    log.info("[main] Shutdown complete. Goodbye.");
    aura::Logger::shutdown();
}

// =============================================================================
// main()
// =============================================================================
int main(int argc, char* argv[]) {
    // -- Step 0: Crash reporter -- must be installed first --------------------
    aura::CrashReporter::install();
    aura::CrashReporter::setAppVersion(AURA_VERSION_STRING);

    // -- COM initialization -- MUST be before any WASAPI / WMF / DirectShow --
    // COINIT_MULTITHREADED: audio capture (WASAPI) and Media Foundation (WMF)
    // both require multi-threaded COM. Qt itself re-initializes COM on its GUI
    // thread with COINIT_APARTMENTTHREADED, which is compatible.
    HRESULT hrCOM = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hrCOM) && hrCOM != RPC_E_CHANGED_MODE) {
        MessageBoxA(nullptr, "COM initialization failed. AuraCastPro cannot start.",
                    "AuraCastPro -- Fatal Error", MB_ICONERROR);
        return 1;
    }
    // Media Foundation startup -- required before any IMFTransform / decoder use
    HRESULT hrMF = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hrMF)) {
        // Non-fatal: hardware decode will fall back to software via FFmpeg
        // (MF may be unavailable on Windows Server Core)
    }
    // Ensure COM + MF are shut down on exit
    struct ComGuard {
        bool mfStarted;
        ComGuard(bool mf) : mfStarted(mf) {
        }
        ~ComGuard() {
            if (mfStarted)
                MFShutdown();
            CoUninitialize();
        }
    } comGuard(SUCCEEDED(hrMF));

    // Qt before QStandardPaths
    QApplication app(argc, argv);
    app.setApplicationName("AuraCastPro");
    app.setApplicationVersion(AURA_VERSION_STRING);
    app.setOrganizationName("AuraCastPro");
    app.setOrganizationDomain("auracastpro.com");

    // Ensure clean shutdown on ANY exit path — X button, Quit menu, Task Manager kill
    // aboutToQuit fires for qApp->quit() and also when Windows terminates the process
    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        AURA_LOG_INFO("main", "aboutToQuit fired -- running shutdownAll()");
        shutdownAll();
    });

    // Handle Ctrl+C / console close / Windows session end
    SetConsoleCtrlHandler(
        [](DWORD) -> BOOL {
            QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
            return TRUE;
        },
        TRUE);

    // Task 178: --minimised flag -- written to registry for auto-start at login.
    // When set, the app starts hidden in the system tray (no main window shown).
    const bool startMinimised =
        app.arguments().contains("--minimised") || app.arguments().contains("--minimized");

    ensureDirectories();

    // -- Step 1: Logging ------------------------------------------------------
    const std::string logDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() + "/logs";

    try {
        aura::Logger::init(logDir, 10, 5);

        // Task 159: Open Windows Event Log (silently skips if source not registered)
        aura::WindowsEventLog::instance().open();
        aura::WindowsEventLog::instance().logAppStart(AURA_VERSION_STRING);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "AuraCastPro -- Logger Failed", MB_ICONERROR);
        return 1;
    }

    AURA_LOG_INFO("main", "=========================================");
    AURA_LOG_INFO("main", " AuraCastPro v{} starting", AURA_VERSION_STRING);
    AURA_LOG_INFO("main", "=========================================");

    aura::PerformanceTimer::init();
    aura::PerformanceTimer startupTimer;
    startupTimer.start();

    // Register app as a Windows Event Log source
    {
        HKEY hKey = nullptr;
        const std::wstring regPath =
            L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\AuraCastPro";
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &hKey, nullptr);
        if (hKey)
            RegCloseKey(hKey);
    }

    // Qt locale/translation
    QTranslator translator;
    if (translator.load(":/i18n/AuraCastPro_en.qm"))
        app.installTranslator(&translator);

    // Apply system proxy settings
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // -- Splash screen --------------------------------------------------------
    QSplashScreen* splash = nullptr;
    {
        QPixmap px(":/textures/8K_Splashes/splash.png");
        if (!px.isNull()) {
            splash = new QSplashScreen(px);
            splash->show();
            app.processEvents();
        }
    }
    auto splashMsg = [&](const QString& msg, int pct) {
        if (splash) {
            splash->showMessage(QString("%1  %2%").arg(msg).arg(pct),
                                Qt::AlignBottom | Qt::AlignLeft, Qt::white);
            app.processEvents();
        }
        AURA_LOG_INFO("main", "Startup [{}%]: {}", pct, msg.toStdString());
    };

    splashMsg("Checking hardware...", 5);

    // -- Step 2: OS + hardware detection -------------------------------------
    SAFE_INIT("OSVersionHelper", aura::OSVersionHelper::detect());
    SAFE_INIT("HardwareProfiler", aura::HardwareProfiler::detect());

    if (!aura::OSVersionHelper::version().isWindows10OrLater) {
        MessageBoxA(nullptr, "Windows 10 or later required.", "Unsupported OS", MB_ICONERROR);
        aura::Logger::shutdown();
        return 1;
    }

    // -- Step 3: Settings -----------------------------------------------------
    splashMsg("Loading settings...", 10);
    SAFE_INIT("SettingsModel", {
        g_settings = std::make_unique<aura::SettingsModel>();
        g_settings->load();
        // Populate read-only system info properties used by SettingsPage "ABOUT" section
        const auto& hw = aura::HardwareProfiler::profile();
        const auto& os = aura::OSVersionHelper::version();
        g_settings->setGpuName(
            QString::fromStdString(hw.primaryGpu ? hw.primaryGpu->name : std::string("Unknown")));
        g_settings->setOsVersion(QString::fromStdString(os.displayName));
    });

    // -- Step 4: Security vault -----------------------------------------------
    SAFE_INIT("SecurityVault", {
        g_vault = std::make_unique<aura::SecurityVault>();
        g_vault->init();
        g_vault->loadTrustedDevices();  // Task 154: load trusted_devices.json on startup
        // Wire vault into settings for encrypted stream key storage
        if (g_settings)
            g_settings->setVault(g_vault.get());
    });

    // -- Step 5: Licensing ----------------------------------------------------
    splashMsg("Checking license...", 15);
    SAFE_INIT("LicenseManager", {
        g_license = std::make_unique<aura::LicenseManager>();
        g_licenseValidator = std::make_unique<aura::LicenseValidator>();
        g_license->init();
        g_licenseValidator->init();
        g_featureGate = std::make_unique<aura::FeatureGate>(g_license.get());
        // Mirror license state into SettingsModel for QML SettingsPage display
        if (g_settings) {
            auto syncLicense = [&]() {
                g_settings->setEdition(g_license->tierName());
                g_settings->setLicenseStatus(g_license->isValid() ? "Active" : "Unlicensed");
            };
            syncLicense();
            // Re-sync when license changes (activate/deactivate)
            QObject::connect(g_license.get(), &aura::LicenseManager::licenseChanged,
                             [syncLicense = std::move(syncLicense)]() mutable { syncLicense(); });
        }
        g_featureGate->init();
    });

    // -- Step 6: DirectX 12 --------------------------------------------------
    splashMsg("Initialising GPU...", 20);
    SAFE_INIT_CRITICAL("DX12Manager", {
        g_dx12 = std::make_unique<aura::DX12Manager>();
        g_dx12->init();
        AURA_LOG_INFO("main", "GPU: {}", g_dx12->adapterName());
    });

    // -- Step 7: Audio pipeline -----------------------------------------------
    splashMsg("Starting audio engine...", 30);
    SAFE_INIT("AudioLoopback", {
        g_audioLoopback = std::make_unique<aura::AudioLoopback>();
        // Task 119: apply saved output device selection before init()
        if (g_settings) {
            const std::string devId = g_settings->outputDeviceId().toStdString();
            if (!devId.empty())
                g_audioLoopback->setOutputDevice(devId);
        }
        g_audioLoopback->init();
    });
    SAFE_INIT("AudioMixer", {
        g_audioMixer = std::make_unique<aura::AudioMixer>();
        g_audioMixer->init();
        // Wire: loopback -> mixer
        if (g_audioLoopback) {
            g_audioLoopback->setCallback([](const aura::AudioBuffer& buf) {
                if (g_audioMixer)
                    g_audioMixer->feedDeviceAudio(buf);
            });
        }
    });
    SAFE_INIT("VirtualAudioDriver", {
        g_virtualAudio = std::make_unique<aura::VirtualAudioDriver>();
        g_virtualAudio->init();
        // Wire: mixer -> virtual audio output
        if (g_audioMixer && g_virtualAudio) {
            g_audioMixer->addOutputSink([](const float* s, uint32_t n, uint32_t sr, uint32_t ch) {
                if (g_virtualAudio)
                    g_virtualAudio->writeSamples(s, n, sr, ch);
            });
        }
    });

    // -- Step 8: Video decode pipeline ----------------------------------------
    splashMsg("Building decode pipeline...", 40);

    // 8a. Video decoder -- created first so downstream can capture its pointer
    SAFE_INIT("VideoDecoder", {
        g_videoDecoder = std::make_unique<aura::VideoDecoder>(g_dx12 ? g_dx12->device() : nullptr,
                                                              aura::VideoCodec::H265);
        g_videoDecoder->init();

        // Wire: decoder frame output -> mirror window + virtual camera
        g_videoDecoder->setFrameCallback([](const aura::DecodedFrame& frame) {
            if (g_mirrorWindow) {
                if (frame.texture)
                    g_mirrorWindow->setCurrentTexture(frame.texture);
                g_mirrorWindow->presentFrame(frame.width, frame.height);
            }
            if (g_vcam && frame.texture) {
                g_vcam->pushFrameGPU(frame.texture, frame.width, frame.height);
            }
            // Feed hardware encoder for streaming / recording output
            if (g_encoder && frame.texture) {
                g_encoder->feedFrame(frame.texture, static_cast<int64_t>(frame.presentationTimeUs));
            }
            // Dispatch decoded frame to all loaded plugins
            if (g_plugins) {
                aura::VideoFrameInfo vfi{};
                vfi.width = frame.width;
                vfi.height = frame.height;
                vfi.nv12Data = nullptr;  // GPU texture; CPU readback not enabled by default
                vfi.ptsUs = frame.presentationTimeUs;
                vfi.isKeyframe = frame.isKeyframe;
                g_plugins->dispatchVideoFrame(vfi);
            }
            // Track FPS and GPU frame time for stats overlay
            if (g_networkStats) {
                static auto lastFrameTime = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                double frameMs =
                    std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
                lastFrameTime = now;
                double fps = frameMs > 0.0 ? 1000.0 / frameMs : 0.0;
                g_networkStats->setFps(fps);
                g_networkStats->setGpuFrameMs(frame.gpuDecodeTimeMs);
                g_networkStats->incrementTotalFrames();
                if (frame.wasDropped)
                    g_networkStats->incrementDroppedFrames();
            }
        });
    });

    // 8b. NAL parser -- constructor requires callback; wire directly to decoder
    SAFE_INIT("NALParser", {
        g_nalParser = std::make_unique<aura::NALParser>([](aura::NalUnit nal) {
            if (!g_videoDecoder)
                return;
            g_videoDecoder->submitNAL(nal.data, nal.isKeyframe, 0 /*pts filled by demuxer*/);
            g_videoDecoder->processOutput();
        });
    });

    SAFE_INIT("H265Demuxer", {
        g_h265Demuxer = std::make_unique<aura::H265Demuxer>();
        g_h265Demuxer->init();
    });
    SAFE_INIT("AV1Demuxer", {
        g_av1Demuxer = std::make_unique<aura::AV1Demuxer>();
        g_av1Demuxer->init();
    });
    SAFE_INIT("FrameTimingQueue", {
        g_frameTiming = std::make_unique<aura::FrameTimingQueue>();
        g_frameTiming->init();
    });
    SAFE_INIT("NetworkPredictor", {
        g_networkPredictor = std::make_unique<aura::NetworkPredictor>();
        g_networkPredictor->init();
    });
    SAFE_INIT("BitratePID", {
        g_bitratePID = std::make_unique<aura::BitratePID>();
        g_bitratePID->reset(20'000'000.f);  // start at 20 Mbps
        g_bitratePID->setOnBitrateChanged(
            [](float bps) { AURA_LOG_DEBUG("main", "Bitrate -> {:.1f} Mbps", bps / 1e6f); });
    });

    // 8c. FEC -- wire to NALParser downstream
    SAFE_INIT("FECRecovery", {
        g_fec = std::make_unique<aura::FECRecovery>(10, 12);
        g_fec->init();
        // Wire: FEC recovered data -> NALParser + update packet loss stats
        g_fec->setCallback([](std::vector<uint8_t> data, uint16_t /*seq*/) {
            if (g_nalParser)
                g_nalParser->feedPacket(std::span<const uint8_t>(data.data(), data.size()));
            // Update packet loss % in stats model every 100 deliveries
            if (g_networkStats && g_fec) {
                static uint64_t lastUpdate = 0;
                if (g_fec->totalPackets() - lastUpdate >= 100) {
                    g_networkStats->setPacketLossPct(g_fec->packetLossPct());
                    // Feed reorder depth and FEC recovery count for DiagnosticsPanel
                    if (g_reorderCache)
                        g_networkStats->setReorderQueueDepth(
                            static_cast<int>(g_reorderCache->queueDepth()));
                    g_networkStats->incrementFecRecoveries();
                    lastUpdate = g_fec->totalPackets();
                }
            }
        });
    });

    // -- Step 9: Network receive pipeline (full hot-path wiring) --------------
    // Data flow:
    //   UDP Socket -> PacketReorderCache -> FECRecovery -> NALParser -> VideoDecoder
    splashMsg("Binding network sockets...", 50);

    // 9a. Reorder cache (no constructor dependencies)
    SAFE_INIT("PacketReorderCache",
              { g_reorderCache = std::make_unique<aura::PacketReorderCache>(256, 50); });

    // 9a.5: ReconnectManager -- auto-reconnect when packets stop arriving
    SAFE_INIT("ReconnectManager", {
        ReconnectManager::Config rcfg;
        rcfg.silenceTimeoutSec = 5;
        rcfg.retryIntervalSec = 3;
        rcfg.maxRetries = 20;
        g_reconnect = std::make_unique<ReconnectManager>(rcfg);
        g_reconnect->setReconnectAttempt([] {
            // Flush pipeline and signal protocol hosts to re-establish
            AURA_LOG_INFO("main", "ReconnectManager: triggering reconnect attempt...");
            if (g_videoDecoder)
                g_videoDecoder->resetState();
            if (g_reorderCache)
                g_reorderCache->flush();
            if (g_fec)
                g_fec->reset();
            if (g_nalParser)
                g_nalParser->reset();
        });
        g_reconnect->setOnGiveUp([] {
            AURA_LOG_WARN("main", "ReconnectManager: max retries reached -- giving up.");
            if (g_deviceManager)
                g_deviceManager->disconnectAll();
        });
        g_reconnect->start();
    });

    // 9b. ReceiverSocket -- FIXED: requires (port, PacketCallback) constructor
    //     Shared_ptr so UDPServerThreadPool also holds a reference
    try {
        // AirPlay video RTP streams arrive on UDP 7010.
        // (UDP 7236 is the RTSP control port -- different from the RTP data port.)
        constexpr uint16_t kAirPlayVideoPort = 7010;

        auto packetCallback = [](aura::RawPacket pkt) {
            if (!g_reorderCache || !g_fec)
                return;

            // -- Notify reconnect manager -- reset silence timer -------------
            if (g_reconnect)
                g_reconnect->onPacketReceived();

            // -- Track receive latency for stats overlay ---------------------
            if (g_networkStats) {
                // Estimate one-way network latency from jitter in arrival times
                static auto prevArrival = std::chrono::steady_clock::now();
                static double smoothedInterPacketMs = 8.3;  // init at ~120fps
                auto now = std::chrono::steady_clock::now();
                double interMs =
                    std::chrono::duration<double, std::milli>(now - prevArrival).count();
                prevArrival = now;
                // EMA smoothed jitter as proxy latency indicator
                smoothedInterPacketMs = 0.05 * interMs + 0.95 * smoothedInterPacketMs;
                g_networkStats->setLatencyMs(smoothedInterPacketMs * 6.0);  // scale to ms
            }

            // -- Build OrderedPacket from raw RTP bytes ----------------------
            // RTP fixed header: 12 bytes
            //   [0]     V(2) P(1) X(1) CC(4)
            //   [1]     M(1) PT(7)
            //   [2-3]   Sequence number (big-endian uint16)
            //   [4-7]   Timestamp
            //   [8-11]  SSRC
            //   [12+]   Payload
            aura::OrderedPacket op;
            op.insertedAt = pkt.receivedAt;

            if (pkt.data.size() >= 4) {
                op.sequenceNumber =
                    static_cast<uint16_t>((static_cast<uint16_t>(pkt.data[2]) << 8) | pkt.data[3]);

                constexpr size_t kRTPHeaderSize = 12;
                if (pkt.data.size() > kRTPHeaderSize) {
                    op.payload.assign(pkt.data.begin() + kRTPHeaderSize, pkt.data.end());
                }
            } else {
                op.sequenceNumber = 0;
                op.payload = std::move(pkt.data);
            }
            op.isKeyframe = false;  // determined by NALParser downstream

            // -- Insert into reorder cache -----------------------------------
            g_reorderCache->insert(std::move(op));

            // -- Drain in-order packets -> FEC -------------------------------
            g_reorderCache->drain([](aura::OrderedPacket ordered) {
                if (!g_fec)
                    return;
                // In an RS(10,12) group: packets 0-9 = data, 10-11 = parity
                const bool isParity = (ordered.sequenceNumber % 12) >= 10;
                g_fec->feedPacket(ordered.sequenceNumber, isParity, std::move(ordered.payload));
            });
        };

        g_sharedSocket = std::make_shared<aura::ReceiverSocket>(kAirPlayVideoPort, packetCallback);
        // Task 074: bind to the selected network interface from settings
        if (g_settings) {
            const std::string iface = g_settings->selectedNetworkIface().toStdString();
            if (!iface.empty())
                g_sharedSocket->setBindInterface(iface);
        }
        AURA_LOG_INFO("main", "ReceiverSocket initialised.");
    } catch (const std::exception& e) {
        AURA_LOG_ERROR("main", "ReceiverSocket init failed: {}", e.what());
    }

    // 9c. Thread pool -- FIXED: takes shared_ptr<ReceiverSocket>
    SAFE_INIT("UDPServerThreadPool", {
        // Use optimal thread count (=4, = half CPU cores)
        g_udpPool = std::make_unique<aura::UDPServerThreadPool>(g_sharedSocket,
                                                                /* numThreads = */ 0 /*auto*/);
        // No init() -- ready after construction
    });

    // -- Step 10: Virtual camera + recording ---------------------------------
    splashMsg("Initialising output pipeline...", 55);
    SAFE_INIT("VCamBridge", {
        g_vcam = std::make_unique<aura::VCamBridge>();
        g_vcam->init();
    });
    SAFE_INIT("StreamRecorder", {
        g_recorder = std::make_unique<aura::StreamRecorder>();
        g_recorder->init();
        // Wire recorder into HubWindow so QML toggle works
        if (g_hubWindow)
            g_hubWindow->setRecorder(g_recorder.get());

        // Wire: AudioMixer mixed output -> recorder (audio track in MP4)
        // Added here (after recorder creation) because AudioMixer SAFE_INIT ran earlier.
        if (g_audioMixer) {
            g_audioMixer->addOutputSink([](const float* samples, uint32_t numFrames,
                                           uint32_t sampleRate, uint32_t channels) {
                if (g_recorder)
                    g_recorder->onAudioSamples(samples, numFrames, sampleRate, channels);
            });
        }
    });
    SAFE_INIT("DiskSpaceMonitor", {
        g_diskMonitor = std::make_unique<aura::DiskSpaceMonitor>();
        g_diskMonitor->init();
        // Apply user-configured disk warning threshold from settings
        if (g_settings) {
            const uint64_t warnBytes =
                static_cast<uint64_t>(g_settings->diskWarnGb()) * 1024ULL * 1024ULL * 1024ULL;
            if (warnBytes > 0)
                g_diskMonitor->setWarningThreshold(warnBytes);
        }
        // Watch the recordings folder on the user's Documents drive
        const std::string recPath =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString() +
            "/AuraCastPro/recordings";
        g_diskMonitor->start(recPath);

        // Low-disk warning -> toast notification
        g_diskMonitor->setLowDiskCallback([](uint64_t freeBytes) {
            const double gb = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
            const QString msg = QString("Only %1 GB left on recording drive.").arg(gb, 0, 'f', 1);
            AURA_LOG_WARN("main", "Low disk space: {} bytes free", freeBytes);
            if (g_hubWindow) {
                QMetaObject::invokeMethod(
                    g_hubWindow.get(),
                    [msg]() {
                        // Show toast via HubModel log line (visible in DiagnosticsPanel)
                        auto* hm = g_hubWindow ? g_hubWindow->hubModel() : nullptr;
                        if (hm)
                            hm->appendLogLine("[? Disk] " + msg);
                    },
                    Qt::QueuedConnection);
            }
        });

        // Critical disk space -> auto-stop recording to prevent corruption
        g_diskMonitor->setCriticalCallback([&]() {
            AURA_LOG_ERROR("main", "Critical disk space -- auto-stopping recording");
            if (g_recorder)
                g_recorder->stopRecording();
            if (g_hubWindow) {
                QMetaObject::invokeMethod(
                    g_hubWindow.get(),
                    []() {
                        auto* hm = g_hubWindow ? g_hubWindow->hubModel() : nullptr;
                        if (hm) {
                            hm->appendLogLine(
                                "[? Disk] Critical -- recording stopped automatically.");
                            hm->setProperty("isRecording", false);
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    });
    SAFE_INIT("RTMPOutput", {
        g_rtmp = std::make_unique<aura::RTMPOutput>();
        g_rtmp->init();
    });
    SAFE_INIT("HardwareEncoder", {
        g_encoder = std::make_unique<aura::HardwareEncoder>();
        g_encoder->init();
        // Wire: encoder output packets -> RTMP + recorder
        g_encoder->setCallback([](const aura::EncodedPacket& ep) {
            if (g_rtmp)
                g_rtmp->sendVideoPacket(ep.data.data(), ep.data.size(), ep.pts, ep.isKeyframe);
            if (g_recorder)
                g_recorder->feedVideoPacket(ep.data.data(), ep.data.size(), ep.pts, ep.isKeyframe);
        });
    });
    SAFE_INIT("NVENCWrapper", {
        g_nvenc = std::make_unique<aura::NVENCWrapper>();
        g_nvenc->init();
    });
    SAFE_INIT("StreamBroadcaster", {
        g_broadcaster =
            std::make_unique<aura::StreamBroadcaster>(g_vcam.get(), g_recorder.get(), g_rtmp.get());
        g_broadcaster->init();
    });

    // -- Step 11: Input control ------------------------------------------------
    SAFE_INIT("MouseToTouch", {
        g_mouseToTouch = std::make_unique<aura::MouseToTouch>();
        g_mouseToTouch->init();
    });
    SAFE_INIT("KeyboardToInput", {
        g_keyboardInput = std::make_unique<aura::KeyboardToInput>();
        g_keyboardInput->init();
    });
    SAFE_INIT("AndroidControlBridge", {
        g_androidControl = std::make_unique<aura::AndroidControlBridge>();
        g_androidControl->init();
    });
    SAFE_INIT("ProtocolHandshake", {
        g_handshake = std::make_unique<aura::ProtocolHandshake>();
        g_handshake->init();
    });

    // Task 150: Clipboard bridge -- bidirectional PC ? device clipboard sync
    SAFE_INIT("ClipboardBridge", {
        g_clipboard = std::make_unique<aura::ClipboardBridge>();
        // Wire to AndroidControlBridge for push-to-device
        g_clipboard->startMonitoring();
        AURA_LOG_INFO("main", "Clipboard bridge started (auto-sync: off until device connects).");
    });

    // Task 062: USB hotplug -- auto-detect Android devices on USB plug-in
    SAFE_INIT("USBHotplug", {
        g_usbHotplug = std::make_unique<aura::USBHotplug>();
        g_usbHotplug->setOnDeviceArrived([] {
            AURA_LOG_INFO("main", "USB device arrived -- triggering ADB scan.");
            if (g_adb)
                g_adb->scanForDevices();
        });
        g_usbHotplug->setOnDeviceRemoved([] {
            AURA_LOG_INFO("main", "USB device removed.");
            // ADB bridge will detect device loss on its own heartbeat
        });
        g_usbHotplug->start();
    });

    // -- Step 12: Protocol hosts -----------------------------------------------
    splashMsg("Starting protocol hosts...", 60);

    SAFE_INIT("AirPlay2Host", {
        g_airplay = std::make_unique<aura::AirPlay2Host>();
        g_airplay->init();

        g_airplay->setSessionStartedCallback([](const aura::AirPlaySessionInfo& info) {
            if (g_reconnect)
                g_reconnect->onConnected(info.deviceId);
            AURA_LOG_INFO("main", "AirPlay session started: {} ({})", info.deviceName,
                          info.ipAddress);
            if (g_deviceManager) {
                aura::DeviceInfo deviceInfo;
                deviceInfo.id = QString::fromStdString(info.deviceId);
                deviceInfo.name = QString::fromStdString(info.deviceName.empty() ? info.ipAddress
                                                                                 : info.deviceName);
                deviceInfo.ipAddress = QString::fromStdString(info.ipAddress);
                deviceInfo.type = aura::DeviceType::IOS;
                deviceInfo.state = aura::DeviceState::Connected;
                deviceInfo.isPaired = info.isPaired;
                g_deviceManager->onDeviceDiscovered(deviceInfo);
                g_deviceManager->onDeviceStateChanged(QString::fromStdString(info.deviceId),
                                                      aura::DeviceState::Streaming);
            }
            if (g_broadcaster)
                g_broadcaster->start();
            if (g_vcam)
                g_vcam->start();
            if (g_audioLoopback)
                g_audioLoopback->start();
            if (g_audioMixer)
                g_audioMixer->start();
            // Whitelist source IP in ReceiverSocket for security
            if (g_sharedSocket && !info.ipAddress.empty())
                g_sharedSocket->setAllowedSource(info.ipAddress);
        });
        g_airplay->setSessionEndedCallback([](const std::string& deviceId) {
            if (g_reconnect)
                g_reconnect->onDisconnected();
            AURA_LOG_INFO("main", "AirPlay session ended: {}", deviceId);
            if (g_deviceManager)
                g_deviceManager->onDeviceStateChanged(QString::fromStdString(deviceId),
                                                      aura::DeviceState::Connected);
            if (g_broadcaster)
                g_broadcaster->stop();
            if (g_audioLoopback)
                g_audioLoopback->stop();
            if (g_sharedSocket)
                g_sharedSocket->clearAllowedSource();
            if (g_videoDecoder)
                g_videoDecoder->resetState();
            if (g_reorderCache)
                g_reorderCache->flush();
            if (g_fec)
                g_fec->reset();
            if (g_nalParser)
                g_nalParser->reset();
        });
        g_airplay->setPinRequestCallback(
            [](const std::string& pin) { AURA_LOG_INFO("main", "AirPlay PIN: {}", pin); });

        // Wire pairing result -> HubModel -> QML PairingDialog
        g_airplay->setPairingResultCallback([](bool success) {
            AURA_LOG_INFO("main", "AirPlay pairing {}", success ? "succeeded" : "failed");
            if (g_hubWindow) {
                // Deliver on the Qt main thread via queued connection
                QMetaObject::invokeMethod(
                    g_hubWindow.get(),
                    [success]() {
                        auto* hm = g_hubWindow ? g_hubWindow->hubModel() : nullptr;
                        if (hm)
                            hm->notifyPairingResult(success);
                    },
                    Qt::QueuedConnection);
            }
        });
    });

    SAFE_INIT("CastV2Host", {
        g_cast = std::make_unique<aura::CastV2Host>();
        g_cast->init();
        g_cast->setSessionStartedCallback([](const aura::CastSessionInfo& info) {
            AURA_LOG_INFO("main", "Cast session started from {}", info.ipAddress);
            if (g_deviceManager) {
                aura::DeviceInfo deviceInfo;
                deviceInfo.id = QString::fromStdString(info.sessionId.empty() ? info.ipAddress
                                                                              : info.sessionId);
                deviceInfo.name = QStringLiteral("Android Cast");
                deviceInfo.ipAddress = QString::fromStdString(info.ipAddress);
                deviceInfo.type = aura::DeviceType::Android;
                deviceInfo.state = aura::DeviceState::Connected;
                g_deviceManager->onDeviceDiscovered(deviceInfo);
                g_deviceManager->onDeviceStateChanged(deviceInfo.id, aura::DeviceState::Streaming);
            }
            if (g_broadcaster)
                g_broadcaster->start();
            if (g_audioLoopback)
                g_audioLoopback->start();
            if (g_audioMixer)
                g_audioMixer->start();
        });
        g_cast->setSessionEndedCallback([](const std::string& /*id*/) {
            if (g_broadcaster)
                g_broadcaster->stop();
            if (g_audioLoopback)
                g_audioLoopback->stop();
        });
    });

    SAFE_INIT("ADBBridge", {
        g_adb = std::make_unique<aura::ADBBridge>();
        g_adb->init();
        g_adb->setDeviceFoundCallback([](const aura::AndroidDevice& dev) {
            AURA_LOG_INFO("main", "ADB device: {} {} ({}x{})", dev.serial, dev.model,
                          dev.screenWidth, dev.screenHeight);
            if (!g_deviceManager)
                return;
            aura::DeviceInfo info;
            info.id = QString::fromStdString(dev.serial);
            info.name = QString::fromStdString(dev.model);
            info.ipAddress = {};
            info.type = aura::DeviceType::Android;
            info.state = aura::DeviceState::Discovered;
            g_deviceManager->onDeviceDiscovered(info);
        });

        // Wire Android authorization result -> HubModel -> QML PairingDialog
        g_adb->setPairingResultCallback([](const std::string& serial, bool success) {
            AURA_LOG_INFO("main", "ADB pairing {} for {}", success ? "succeeded" : "failed",
                          serial);
            if (g_hubWindow) {
                QMetaObject::invokeMethod(
                    g_hubWindow.get(),
                    [success]() {
                        auto* hm = g_hubWindow ? g_hubWindow->hubModel() : nullptr;
                        if (hm)
                            hm->notifyPairingResult(success);
                    },
                    Qt::QueuedConnection);
            }
        });
    });

    // -- Step 13: mDNS / network advertising ----------------------------------
    splashMsg("Advertising on network...", 70);
    SAFE_INIT("BonjourAdapter", {
        g_bonjour = std::make_unique<aura::BonjourAdapter>();
        g_bonjour->init();
    });
    // Warn user if Bonjour is not installed (required for AirPlay discovery)
    if (g_bonjour && !g_bonjour->isAvailable()) {
        AURA_LOG_WARN(
            "main",
            "Bonjour (Apple mDNS) is not installed. "
            "AirPlay device discovery will be unavailable. "
            "Install Bonjour for Windows from https://support.apple.com/downloads/bonjour");
        aura::ToastNotification::show(
            "AirPlay Discovery Unavailable",
            "Bonjour is not installed. Apple AirPlay devices will not be discovered. "
            "Install Bonjour for Windows to enable AirPlay mirroring.",
            aura::ToastIcon::Warning);
    }

    SAFE_INIT("MDNSService", {
        g_mdns = std::make_unique<aura::MDNSService>();
        g_mdns->init();
        const std::string displayName =
            g_settings ? g_settings->displayName().toStdString() : "AuraCastPro";
        g_mdns->setDisplayName(displayName);
    });
    SAFE_INIT("DeviceAdvertiser", {
        g_advertiser = std::make_unique<aura::DeviceAdvertiser>(g_bonjour.get());
        g_advertiser->init();
        if (g_settings)
            g_advertiser->setDisplayName(g_settings->displayName().toStdString());
    });

    // -- Step 14: Mirror window + render loop ---------------------------------
    splashMsg("Creating mirror window...", 75);
    SAFE_INIT("MirrorWindow", {
        g_mirrorWindow = std::make_unique<aura::MirrorWindow>(g_dx12.get(), g_vcam.get());
        g_mirrorWindow->init();
    });

    // -- Step 15: Device manager -----------------------------------------------
    SAFE_INIT("DeviceManager", {
        g_deviceManager = std::make_unique<aura::DeviceManager>();
        g_deviceManager->init();
        // Wire DeviceManager scan/mirror signals to protocol hosts
        QObject::connect(g_deviceManager.get(), &aura::DeviceManager::scanRequested, []() {
            if (g_mdns)
                g_mdns->start();  // re-advertise on rescan
        });
        QObject::connect(
            g_deviceManager.get(), &aura::DeviceManager::mirrorRequested,
            [](const QString& deviceId) {
                if (g_adb) {
                    g_adb->startMirroring(
                        deviceId.toStdString(), [](const uint8_t* data, size_t len, bool isKey) {
                            if (!g_nalParser || len == 0)
                                return;
                            g_nalParser->feedPacket(std::span<const uint8_t>(data, len));
                        });
                }
            });
    });

    // -- Step 16: Network stats + overlay -------------------------------------
    SAFE_INIT("NetworkStatsModel", {
        g_networkStats = std::make_unique<aura::NetworkStatsModel>();
        g_networkStats->init();
        // Feed stats from BitratePID callback
        if (g_bitratePID) {
            g_bitratePID->setOnBitrateChanged([](float bps) {
                if (g_networkStats)
                    g_networkStats->setCurrentBitrateBps(bps);
            });
        }
    });
    SAFE_INIT("PerformanceOverlay", {
        g_overlay = std::make_unique<aura::PerformanceOverlay>(g_networkStats.get());
        g_overlay->init();
        if (g_mirrorWindow)
            g_mirrorWindow->setOverlay(g_overlay.get());
    });

    // -- Step 17: Plugins -----------------------------------------------------
    SAFE_INIT("PluginManager", {
        g_plugins = std::make_unique<aura::PluginManager>();
        g_plugins->init();
        const std::string pluginDir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString() +
            "/plugins";
        g_plugins->discoverAndLoad(pluginDir);
    });

    // -- Step 18: Cloud services -----------------------------------------------
    SAFE_INIT("TelemetryClient", {
        g_telemetry = std::make_unique<aura::TelemetryClient>();
        g_telemetry->init();
        g_telemetry->setEnabled(false);  // default off; toggled by settings
    });
    SAFE_INIT("LicenseClient", {
        g_licenseClient = std::make_unique<aura::LicenseClient>();
        g_licenseClient->init();
    });
    SAFE_INIT("UpdateService", {
        g_updater = std::make_unique<aura::UpdateService>();
        g_updater->init();
        // Wire update notification -> toast so the user sees an update badge
        g_updater->setUpdateAvailableCallback([](const aura::UpdateInfo& info) {
            AURA_LOG_INFO("main", "Update available: v{}", info.latestVersion);
            aura::ToastNotification::showUpdateAvailable(info.latestVersion, info.downloadUrl);
        });
        g_updater->startAutoCheck(AURA_VERSION_STRING);
    });

    // -- Step 19: Start network services --------------------------------------
    splashMsg("Starting services...", 80);
    SAFE_INIT("MDNSService.start", {
        if (g_mdns)
            g_mdns->start();
    });
    SAFE_INIT("DeviceAdvertiser.start", {
        if (g_advertiser)
            g_advertiser->start();
    });
    SAFE_INIT("AirPlay2Host.start", {
        if (g_airplay)
            g_airplay->start();
    });
    SAFE_INIT("CastV2Host.start", {
        if (g_cast)
            g_cast->start();
    });
    SAFE_INIT("ADBBridge.start", {
        if (g_adb)
            g_adb->start();
    });
    SAFE_INIT("NetworkStatsModel.start", {
        if (g_networkStats)
            g_networkStats->start();
    });
    // Socket must be bound before thread pool starts calling receiveOnce()
    SAFE_INIT("ReceiverSocket.start", {
        if (g_sharedSocket)
            g_sharedSocket->start();
    });
    SAFE_INIT("UDPPool.start", {
        if (g_udpPool)
            g_udpPool->start();
    });
    // MirrorWindow should NOT be started automatically here; it's started when a device connects.

    // -- Step 20: Main hub window ----------------------------------------------
    splashMsg("Starting user interface...", 90);
    SAFE_INIT("HubWindow", {
        g_hubWindow = std::make_unique<aura::HubWindow>();
        g_hubWindow->init(g_settings.get(), g_deviceManager.get(), g_networkStats.get(),
                          g_license.get(), g_overlay.get());

        // Task 178: If --minimised flag is set, start in tray only (no main window)
        if (!startMinimised)
            g_hubWindow->show();
        else
            AURA_LOG_INFO("main", "Started with --minimised: window hidden, tray active.");

        // Task 176: Initialise toast notifications after QApplication is ready
        aura::ToastNotification::init("AuraCastPro.App");

        // Task 175: Register global hotkeys after main window exists
        aura::GlobalHotkey::instance().registerDefaults();
        aura::GlobalHotkey::instance().setCallback([](aura::HotkeyId id) {
            switch (id) {
                case aura::HotkeyId::ToggleMirrorWindow:
                    if (g_mirrorWindow) {
                        g_mirrorWindow->isFullscreen() ? g_mirrorWindow->hide()
                                                       : g_mirrorWindow->show();
                    }
                    break;
                case aura::HotkeyId::ToggleFullscreen:
                    if (g_mirrorWindow)
                        g_mirrorWindow->toggleFullscreen();
                    break;
                case aura::HotkeyId::Screenshot:
                    // FrameCapture fires via InputExtras -- signal via DeviceManager
                    break;
                case aura::HotkeyId::ToggleRecording:
                    if (g_hubWindow)
                        g_hubWindow->triggerRecordingToggle();
                    break;
                case aura::HotkeyId::Disconnect:
                    if (g_deviceManager)
                        g_deviceManager->disconnectAll();
                    break;
            }
        });

        // Task 177: Register QML error dialog bridge
        if (g_hubWindow) {
            aura::ErrorDialog::setQmlBridge(static_cast<void*>(g_hubWindow.get()));
        }

        // Task 178: Honour "start with Windows" setting -- sync registry on every launch
        if (g_settings) {
            const bool startWithWindows = g_settings->startWithWindows();
            if (startWithWindows)
                WindowsAutoStart::enable();
            else
                WindowsAutoStart::disable();
            // React to in-session changes (user toggles Setting)
            QObject::connect(g_settings.get(), &aura::SettingsModel::uiChanged, g_hubWindow.get(),
                             [] {
                                 if (!g_settings)
                                     return;
                                 g_settings->startWithWindows() ? WindowsAutoStart::enable()
                                                                : WindowsAutoStart::disable();
                             });
        }
    });

    // -- Wire HubModel -> protocol host signals ---------------------------------
    // QML calls hubModel.startMirroring() / stopMirroring() -- connect these to
    // the actual protocol engines so the UI can actually control mirroring.
    if (g_hubWindow) {
        aura::HubModel* hub = g_hubWindow->hubModel();
        if (hub) {
            // Start mirroring: notify each active protocol host
            QObject::connect(
                hub, &aura::HubModel::mirroringStartRequested, hub,
                [] {
                    if (g_airplay)
                        g_airplay->setMirroringActive(true);
                    if (g_cast)
                        g_cast->setMirroringActive(true);
                    if (g_adb)
                        g_adb->setMirroringActive(true);
                    AURA_LOG_INFO("main", "Mirroring start requested -- all hosts notified.");
                },
                Qt::QueuedConnection);

            // Stop mirroring: halt active sessions
            QObject::connect(
                hub, &aura::HubModel::mirroringStopRequested, hub,
                [] {
                    if (g_airplay)
                        g_airplay->setMirroringActive(false);
                    if (g_cast)
                        g_cast->setMirroringActive(false);
                    if (g_adb)
                        g_adb->setMirroringActive(false);
                    AURA_LOG_INFO("main", "Mirroring stop requested -- all hosts notified.");
                },
                Qt::QueuedConnection);

            // Wire LicenseManager to HubModel for QML activation
            hub->setLicenseManager(g_license.get());

            // Android wireless ADB pairing: "address:port|code" format from QML
            // QML emits pin as "<address:port>|<6-digit-code>" (pipe separator)
            QObject::connect(
                hub, &aura::HubModel::pairingPinSubmitted, hub,
                [](const QString& pin) {
                    if (!g_adb)
                        return;
                    const auto parts = pin.split('|');
                    if (parts.size() == 2) {
                        const std::string addr = parts[0].toStdString();
                        const std::string code = parts[1].toStdString();
                        std::thread([addr, code] { g_adb->pairWireless(addr, code); }).detach();
                    } else {
                        AURA_LOG_WARN("main", "pairingPinSubmitted: unexpected format '{}'",
                                      pin.toStdString());
                    }
                },
                Qt::QueuedConnection);
        }
    }

    // -- Final splash close ----------------------------------------------------
    splashMsg("Ready.", 100);
    if (splash) {
        splash->finish(g_hubWindow ? static_cast<QWidget*>(g_hubWindow.get()) : nullptr);
        delete splash;
        splash = nullptr;
    }

    // Apply telemetry opt-in from loaded settings
    if (g_telemetry && g_settings)
        g_telemetry->setEnabled(g_settings->telemetryEnabled());

    const double startupMs = startupTimer.elapsedMs();
    AURA_LOG_INFO("main", "Startup complete in {:.0f} ms", startupMs);
    AURA_LOG_INFO("main", "Visible on network as '{}'.",
                  g_settings ? g_settings->displayName().toStdString() : "AuraCastPro");

    if (g_telemetry && g_telemetry->isEnabled()) {
        g_telemetry->reportStartup(g_dx12 ? g_dx12->adapterName() : "Unknown GPU",
                                   aura::OSVersionHelper::version().displayName);
    }

    // -- Qt event loop ---------------------------------------------------------
    const int exitCode = app.exec();

    AURA_LOG_INFO("main", "Qt event loop exited ({}). Shutting down...", exitCode);
    aura::WindowsEventLog::instance().logAppStop();  // Task 159
    aura::CrashReporter::uninstall();
    shutdownAll();
    return exitCode;
}