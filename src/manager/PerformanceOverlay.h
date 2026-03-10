#pragma once
// =============================================================================
// PerformanceOverlay.h — Task 172: On-screen stats overlay (Ctrl+Shift+O)
//
// Bound into QML context as "performanceOverlay". QML StatsOverlay.qml
// reads isVisible() and triggers repaints via overlayDataReady() signal.
//
// CPU%:     measured via GetSystemTimes() delta every 100ms
// Memory:   measured via GetProcessMemoryInfo() every 100ms
// GPU ms:   fed by DX12CommandQueue after each Present()
// Other stats: read directly from NetworkStatsModel
// =============================================================================
#pragma once
#include <QObject>
#include <atomic>
#include <cstdint>

struct FILETIME;
class QTimer;

namespace aura { class NetworkStatsModel; }

namespace aura {

class PerformanceOverlay : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool   visible       READ isVisible  WRITE setVisible NOTIFY visibilityChanged)
    Q_PROPERTY(double cpuUsagePct   READ cpuUsagePct   NOTIFY overlayDataReady)
    Q_PROPERTY(double workingSetMB  READ workingSetMB  NOTIFY overlayDataReady)
    Q_PROPERTY(double gpuFrameMs    READ gpuFrameMs    NOTIFY overlayDataReady)

public:
    explicit PerformanceOverlay(NetworkStatsModel* stats, QObject* parent = nullptr);
    ~PerformanceOverlay();

    void init();
    void start();
    void stop();
    void shutdown();

    bool   isVisible()    const { return m_visible.load(); }
    void   setVisible(bool v);
    void   toggle()             { setVisible(!m_visible.load()); }

    // Accessors for QML Q_PROPERTYs
    double cpuUsagePct()  const { return m_cpuUsagePct;  }
    double workingSetMB() const { return m_workingSetMB; }
    double gpuFrameMs()   const { return m_gpuFrameMs;   }

    // Called by DX12CommandQueue after each frame Present()
    void setGpuFrameMs(double ms) { m_gpuFrameMs = ms; }

signals:
    void visibilityChanged(bool visible);
    void overlayDataReady();   // emitted every 100ms while visible

private:
    void snapshotCPUTimes();
    void updateCPUUsage();
    void updateMemoryUsage();
    uint64_t ftToU64(const FILETIME& ft) const;

    NetworkStatsModel* m_stats{nullptr};
    QTimer*            m_refreshTimer{nullptr};

    std::atomic<bool>  m_visible{false};

    // CPU tracking
    uint64_t m_prevIdleTime{0};
    uint64_t m_prevKernelTime{0};
    uint64_t m_prevUserTime{0};

    // Displayed metrics
    double m_cpuUsagePct{0.0};
    double m_workingSetMB{0.0};
    double m_gpuFrameMs{0.0};
};

} // namespace aura
