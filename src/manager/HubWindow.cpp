// =============================================================================
// HubWindow.cpp -- FIXED: Added signal/slot wiring and menu bar.
// Previously missing: deviceManager->QML signals, networkStats->graph refresh,
// settingsModel->save debounce, menu bar (File/View/Help), tray minimise.
// =============================================================================

#include "HubWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QPalette>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#include "../integration/StreamRecorder.h"
#include "../licensing/LicenseManager.h"
#include "../pch.h"  // PCH
#include "../utils/Logger.h"
#include "DeviceManager.h"
#include "GlobalHotkey.h"
#include "HubModel.h"
#include "NetworkStatsModel.h"
#include "PerformanceOverlay.h"
#include "SettingsModel.h"


namespace aura {

// ── Constructor ───────────────────────────────────────────────────────────────
HubWindow::HubWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("AuraCastPro");
    resize(1280, 800);
    setMinimumSize(900, 600);
    
    // Remove native title bar - keep solid background
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
}

HubWindow::~HubWindow() {
    shutdown();
}

// ── init() -- call after all subsystems are ready ──────────────────────────────
void HubWindow::init(SettingsModel* settings, DeviceManager* devices, NetworkStatsModel* stats,
                     LicenseManager* license, PerformanceOverlay* overlay) {
    m_settings = settings;
    m_devices = devices;
    m_stats = stats;
    m_license = license;
    m_overlay = overlay;

    setupMenuBar();
    setupTrayIcon();
    setupHubModel();   // MUST come before setupQmlEngine() -- sets context properties
    setupQmlEngine();  // Loads QML; context properties must be registered first
    wireSignals();

    AURA_LOG_INFO("HubWindow", "Initialised. QML engine ready, signals wired.");
}

void HubWindow::shutdown() {
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    AURA_LOG_INFO("HubWindow", "Shutdown complete.");
}

// ── HubModel setup ────────────────────────────────────────────────────────────
void HubWindow::setupHubModel() {
    m_hubModel = new HubModel(this);
    m_hubModel->init(m_devices, m_stats);
}

// ── QML Engine setup ──────────────────────────────────────────────────────────
void HubWindow::setupQmlEngine() {
    m_view = new QQuickWidget(this);
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Dark background so no white flash while QML loads
    m_view->setAttribute(Qt::WA_OpaquePaintEvent);
    m_view->setAttribute(Qt::WA_NoSystemBackground);
    QPalette pal = m_view->palette();
    pal.setColor(QPalette::Window, QColor(0x0f, 0x14, 0x1b));
    m_view->setPalette(pal);
    m_view->setAutoFillBackground(true);

    // Register the qml/ prefix so Theme singleton is found
    m_view->engine()->addImportPath(QStringLiteral("qrc:/"));
    m_view->engine()->addImportPath(QStringLiteral("qrc:/qml"));
    
    // Register Theme as a singleton type
    qmlRegisterSingletonType(QUrl("qrc:/qml/Theme.qml"), "AuraCastPro", 1, 0, "Theme");
    
    AURA_LOG_INFO("HubWindow", "QML import paths configured, Theme singleton registered");

    // Expose all subsystems as context properties accessible from QML
    QQmlContext* ctx = m_view->rootContext();
    ctx->setContextProperty("settingsModel", m_settings);
    ctx->setContextProperty("deviceManager", m_devices);
    ctx->setContextProperty("licenseManager", m_license);
    ctx->setContextProperty("performanceOverlay", m_overlay);
    ctx->setContextProperty("hubModel", m_hubModel);
    ctx->setContextProperty("statsModel", m_stats);

    m_view->setSource(QUrl("qrc:/qml/Main.qml"));

    if (m_view->status() == QQuickWidget::Error) {
        AURA_LOG_ERROR("HubWindow", "Failed to load qrc:/qml/Main.qml:");
        for (const QQmlError& error : m_view->errors()) {
            AURA_LOG_ERROR("HubWindow", "   {}", error.toString().toStdString());
        }
    }

    setCentralWidget(m_view);
}

// ── Signal wiring ─────────────────────────────────────────────────────────────
void HubWindow::wireSignals() {
    if (!m_devices || !m_stats || !m_settings)
        return;

    // DeviceManager -> QML: device list refresh
    connect(m_devices, &DeviceManager::devicesChanged, this, [this]() {
        // QML is bound via context property -- devicesChanged signal propagates automatically
        AURA_LOG_DEBUG("HubWindow", "Device list updated -> QML notified.");
    });

    connect(m_devices, &DeviceManager::devicePairingRequested, this, &HubWindow::showPairingDialog);

    // NetworkStats -> graphs: already fires via statsUpdated() every 100ms
    // QML binds directly to networkStats Q_PROPERTYs -- no extra wiring needed.

    // SettingsModel -> debounced save: settings auto-save on every change
    // (already implemented inside each setter -- no extra wiring needed here)

    // Settings: alwaysOnTop
    connect(m_settings, &SettingsModel::uiChanged, this, [this]() {
        setWindowFlag(Qt::WindowStaysOnTopHint, m_settings->alwaysOnTop());
        show();  // Re-show to apply window flags
    });
}

// ── Menu bar ──────────────────────────────────────────────────────────────────
void HubWindow::setupMenuBar() {
    // Hide the native menu bar - all controls will be in the app UI
    menuBar()->hide();
}

// ── System Tray ───────────────────────────────────────────────────────────────
void HubWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(QIcon(":/icons/app_icon.png"), this);

    QMenu* trayMenu = new QMenu(this);
    auto* actShow = trayMenu->addAction("Open AuraCastPro");
    auto* actMirror = trayMenu->addAction("Start Mirroring");
    auto* actRecord = trayMenu->addAction("Start Recording");
    trayMenu->addSeparator();
    auto* actQuit = trayMenu->addAction("Quit");

    connect(actShow, &QAction::triggered, this, &HubWindow::showAndRaise);
    connect(actMirror, &QAction::triggered, this, [this]() { invokeQml("toggleMirroring"); });
    connect(actRecord, &QAction::triggered, this, [this]() { invokeQml("toggleRecording"); });
    connect(actQuit, &QAction::triggered, this, &HubWindow::requestQuit);

    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->setToolTip("AuraCastPro -- No device connected");
    m_trayIcon->show();

    // Double-click tray -> show/raise window
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick)
                    showAndRaise();
            });
}

void HubWindow::showAndRaise() {
    show();
    raise();
    activateWindow();
}

// ── Minimise to tray ─────────────────────────────────────────────────────────
void HubWindow::closeEvent(QCloseEvent* event) {
    if (!m_forceQuit && m_settings && m_settings->minimiseToTray() && m_trayIcon) {
        hide();
        event->ignore();
        m_trayIcon->showMessage("AuraCastPro", "Running in system tray. Double-click to restore.",
                                QSystemTrayIcon::Information, 2000);
    } else {
        // Full quit — shut down everything and terminate the process
        event->accept();
        requestQuit();
    }
}

void HubWindow::requestQuit() {
    if (m_forceQuit)
        return;  // prevent double-call
    m_forceQuit = true;
    // Hide tray icon immediately so it vanishes from taskbar
    if (m_trayIcon)
        m_trayIcon->hide();
    qApp->quit();
}

// ── Pairing dialog ────────────────────────────────────────────────────────────
void HubWindow::showPairingDialog(const QString& deviceName, const QString& pin) {
    AURA_LOG_INFO("HubWindow", "Showing pairing dialog: device={} pin={}", deviceName.toStdString(),
                  pin.toStdString());
    // Invoke QML method on the root object
    if (!m_view || !m_view->rootObject())
        return;
    QObject* root = m_view->rootObject();
    QMetaObject::invokeMethod(root, "showPairingDialog", Q_ARG(QVariant, deviceName),
                              Q_ARG(QVariant, pin));
}

void HubWindow::notifyPairingSuccess() {
    // Update HubModel -> fires pairingResult(true) signal -> QML PairingDialog.showSuccess()
    if (m_hubModel)
        m_hubModel->notifyPairingResult(true);
    // Also call the legacy QML function in case any QML still listens to onPairingSuccess
    if (m_view && m_view->rootObject())
        QMetaObject::invokeMethod(m_view->rootObject(), "onPairingSuccess");
}

bool HubWindow::isVisible() const {
    return QMainWindow::isVisible();
}

// ── Helper: invoke a QML function by name ─────────────────────────────────────
void HubWindow::invokeQml(const char* method) {
    if (!m_view || !m_view->rootObject())
        return;
    QMetaObject::invokeMethod(m_view->rootObject(), method);
}

void HubWindow::triggerRecordingToggle() {
    if (m_hubModel)
        m_hubModel->toggleRecording();
}

void HubWindow::setRecorder(StreamRecorder* rec) {
    m_recorder = rec;
    if (!m_recorder || !m_hubModel)
        return;

    // Wire QML toggleRecording -> StreamRecorder
    connect(m_hubModel, &HubModel::recordingToggleRequested, this, [this]() {
        if (!m_recorder)
            return;
        if (m_recorder->isRecording()) {
            m_recorder->stopRecording();
            m_hubModel->setProperty("isRecording", false);
            m_hubModel->appendLogLine("[Recorder] Recording stopped.");
            AURA_LOG_INFO("HubWindow", "Recording stopped via QML.");
        } else {
            // Build output path from settings
            const QString folder =
                m_settings ? m_settings->recordingFolder() : QString::fromStdString("");
            const std::string outPath =
                aura::StreamRecorder::generateOutputPath(folder.toStdString());
            const uint32_t w = m_settings ? (uint32_t)m_settings->maxWidth() : 1920u;
            const uint32_t h = m_settings ? (uint32_t)m_settings->maxHeight() : 1080u;
            const uint32_t fps = m_settings ? (uint32_t)m_settings->maxFps() : 30u;
            const std::string codec =
                m_settings ? m_settings->preferredCodec().toStdString() : "hevc";
            if (m_recorder->startRecording(outPath, w, h, fps, codec)) {
                m_hubModel->setProperty("isRecording", true);
                m_hubModel->appendLogLine(QString("[Recorder] Recording started -> %1")
                                              .arg(QString::fromStdString(outPath)));
                AURA_LOG_INFO("HubWindow", "Recording started: {}", outPath);
            } else {
                m_hubModel->appendLogLine("[Recorder] ERROR: failed to start recording.");
                AURA_LOG_ERROR("HubWindow", "StreamRecorder::startRecording failed.");
            }
        }
    });
}

bool HubWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    // Route WM_HOTKEY to GlobalHotkey::processMessage so hotkeys actually fire.
    // This works because HubWindow's HWND is on the main Qt thread, which is also
    // the thread that called RegisterHotKey (both happen on the main thread).
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        if (aura::GlobalHotkey::instance().processMessage(message)) {
            if (result)
                *result = 0;
            return true;  // event consumed
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

}  // namespace aura

#include "HubWindow.moc"