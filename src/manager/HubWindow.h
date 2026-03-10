#pragma once
// =============================================================================
// HubWindow.h — FIXED: Full HubWindow with signals, menu bar, tray icon.
// =============================================================================
#include <QMainWindow>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <QString>

// Forward declarations
class QCloseEvent;

namespace aura {
class StreamRecorder;
class HubModel;
class SettingsModel;
class DeviceManager;
class NetworkStatsModel;
class LicenseManager;
class PerformanceOverlay;

class HubWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit HubWindow(QWidget* parent = nullptr);
    ~HubWindow();

    void init(SettingsModel*      settings,
              DeviceManager*      devices,
              NetworkStatsModel*  stats,
              LicenseManager*     license,
              PerformanceOverlay* overlay);

    void shutdown();

    // Invoked by protocol hosts when pairing is requested
    Q_INVOKABLE void showPairingDialog(const QString& deviceName, const QString& pin);
    Q_INVOKABLE void notifyPairingSuccess();

    bool isVisible() const;

    // Accessor for main.cpp to wire protocol callbacks → HubModel signals
    HubModel* hubModel() const { return m_hubModel; }

protected:
    void closeEvent(QCloseEvent* event) override;
    // Catches WM_HOTKEY messages from registered GlobalHotkeys
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    aura::StreamRecorder* m_recorder{nullptr};
    void setupMenuBar();
    void setupTrayIcon();
    void setupQmlEngine();
    void setupHubModel();
    void wireSignals();
    void setRecorder(aura::StreamRecorder* rec);
    void triggerRecordingToggle();  // called from hotkey handler in main()
    void showAndRaise();
    void invokeQml(const char* method);

    QQmlApplicationEngine* m_engine{nullptr};
    QSystemTrayIcon*        m_trayIcon{nullptr};

    // Non-owning pointers to subsystems (owned by main.cpp)
    SettingsModel*      m_settings{nullptr};
    DeviceManager*      m_devices{nullptr};
    NetworkStatsModel*  m_stats{nullptr};
    LicenseManager*     m_license{nullptr};
    PerformanceOverlay* m_overlay{nullptr};
    HubModel*           m_hubModel{nullptr};
};

} // namespace aura
