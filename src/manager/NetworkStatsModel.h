#pragma once
// =============================================================================
// NetworkStatsModel.h -- Live network statistics for QML binding.
// FIXED: Added 9 missing Q_PROPERTYs + expanded history to 300 samples.
//   Added: averageBitrateKbps, peakBitrateKbps, averageLatencyMs,
//          minLatencyMs, maxLatencyMs, jitterMs, droppedFrames,
//          totalFrames, latencyHistory (300 samples)
// =============================================================================
#include <QObject>
#include <QTimer>
#include <QList>
#include <atomic>
#include <mutex>

namespace aura {

class NetworkStatsModel : public QObject {
    Q_OBJECT

    // ── Current values ────────────────────────────────────────────────────────
    Q_PROPERTY(double latencyMs       READ latencyMs       NOTIFY statsUpdated)
    Q_PROPERTY(double bitrateKbps     READ bitrateKbps     NOTIFY statsUpdated)
    Q_PROPERTY(double fps             READ fps             NOTIFY statsUpdated)
    Q_PROPERTY(double packetLossPct   READ packetLossPct   NOTIFY statsUpdated)
    Q_PROPERTY(double gpuFrameMs      READ gpuFrameMs      NOTIFY statsUpdated)
    Q_PROPERTY(double decodeTimeMs    READ decodeTimeMs    NOTIFY statsUpdated)
    Q_PROPERTY(double gpuFrameTimeMs  READ gpuFrameTimeMs  NOTIFY statsUpdated)
    Q_PROPERTY(int    reorderQueueDepth READ reorderQueueDepth NOTIFY statsUpdated)
    Q_PROPERTY(int    fecRecoveries   READ fecRecoveries   NOTIFY statsUpdated)

    // ── Aggregate values (ADDED) ──────────────────────────────────────────────
    Q_PROPERTY(double averageBitrateKbps READ averageBitrateKbps NOTIFY statsUpdated)
    Q_PROPERTY(double peakBitrateKbps    READ peakBitrateKbps    NOTIFY statsUpdated)
    Q_PROPERTY(double averageLatencyMs   READ averageLatencyMs   NOTIFY statsUpdated)
    Q_PROPERTY(double minLatencyMs       READ minLatencyMs       NOTIFY statsUpdated)
    Q_PROPERTY(double maxLatencyMs       READ maxLatencyMs       NOTIFY statsUpdated)
    Q_PROPERTY(double jitterMs           READ jitterMs           NOTIFY statsUpdated)
    Q_PROPERTY(qint64 droppedFrames      READ droppedFrames      NOTIFY statsUpdated)
    Q_PROPERTY(qint64 totalFrames        READ totalFrames        NOTIFY statsUpdated)

    // ── History arrays (300 samples = 30s at 10Hz) ────────────────────────────
    Q_PROPERTY(QList<double> bitrateHistory READ bitrateHistory NOTIFY statsUpdated)
    Q_PROPERTY(QList<double> latencyHistory READ latencyHistory NOTIFY statsUpdated)

public:
    explicit NetworkStatsModel(QObject* parent = nullptr);

    void init();
    void start();
    void stop();
    void shutdown();

    // Thread-safe update -- called from network/decoder threads
    void updateStats(double latencyMs, double bitrateKbps,
                     double fps, double packetLossPct, double gpuFrameMs);
    void incrementDroppedFrames();
    void incrementTotalFrames();
    void resetStats();

    // ── Convenience partial setters (called from hot-path callbacks) ──────────
    // Allows individual subsystems to update just their own metric.
    void setCurrentBitrateBps(double bps) {
        updateStats(m_latencyMs.load(),
                    bps / 1000.0,           // convert bps -> kbps
                    m_fps.load(),
                    m_packetLossPct.load(),
                    m_gpuFrameMs.load());
    }
    void setLatencyMs(double ms) {
        updateStats(ms, m_bitrateKbps.load(), m_fps.load(),
                    m_packetLossPct.load(), m_gpuFrameMs.load());
    }
    void setFps(double fps) {
        updateStats(m_latencyMs.load(), m_bitrateKbps.load(), fps,
                    m_packetLossPct.load(), m_gpuFrameMs.load());
    }
    void setPacketLossPct(double pct) {
        updateStats(m_latencyMs.load(), m_bitrateKbps.load(),
                    m_fps.load(), pct, m_gpuFrameMs.load());
    }
    void setGpuFrameMs(double ms) {
        updateStats(m_latencyMs.load(), m_bitrateKbps.load(),
                    m_fps.load(), m_packetLossPct.load(), ms);
    }
    void setDecodeTimeMs(double ms)     { m_decodeTimeMs.store(ms); }
    void setGpuFrameTimeMs(double ms)   { m_gpuFrameTimeMs.store(ms); }
    void setReorderQueueDepth(int n)    { m_reorderQueueDepth.store(n); }
    void incrementFecRecoveries()       { m_fecRecoveries.fetch_add(1); }

    // ── Getters ───────────────────────────────────────────────────────────────
    double latencyMs()         const { return m_latencyMs.load(); }
    double bitrateKbps()       const { return m_bitrateKbps.load(); }
    double fps()               const { return m_fps.load(); }
    double packetLossPct()     const { return m_packetLossPct.load(); }
    double gpuFrameMs()        const { return m_gpuFrameMs.load(); }
    double decodeTimeMs()      const { return m_decodeTimeMs.load(); }
    double gpuFrameTimeMs()    const { return m_gpuFrameTimeMs.load(); }
    int    reorderQueueDepth() const { return m_reorderQueueDepth.load(); }
    int    fecRecoveries()     const { return m_fecRecoveries.load(); }

    double averageBitrateKbps() const { return m_avgBitrateKbps.load(); }
    double peakBitrateKbps()    const { return m_peakBitrateKbps.load(); }
    double averageLatencyMs()   const { return m_avgLatencyMs.load(); }
    double minLatencyMs()       const { return m_minLatencyMs.load(); }
    double maxLatencyMs()       const { return m_maxLatencyMs.load(); }
    double jitterMs()           const { return m_jitterMs.load(); }
    qint64 droppedFrames()      const { return m_droppedFrames.load(); }
    qint64 totalFrames()        const { return m_totalFrames.load(); }

    QList<double> bitrateHistory() const;
    QList<double> latencyHistory() const;

signals:
    void statsUpdated();

private slots:
    void onUiRefreshTimer();

private:
    // Current (atomics -- written from any thread)
    std::atomic<double> m_latencyMs{0};
    std::atomic<double> m_bitrateKbps{0};
    std::atomic<double> m_fps{0};
    std::atomic<double> m_packetLossPct{0};
    std::atomic<double> m_gpuFrameMs{0};
    std::atomic<double> m_decodeTimeMs{0};
    std::atomic<double> m_gpuFrameTimeMs{0};
    std::atomic<int>    m_reorderQueueDepth{0};
    std::atomic<int>    m_fecRecoveries{0};

    // Aggregate (atomics)
    std::atomic<double> m_avgBitrateKbps{0};
    std::atomic<double> m_peakBitrateKbps{0};
    std::atomic<double> m_avgLatencyMs{0};
    std::atomic<double> m_minLatencyMs{9999};
    std::atomic<double> m_maxLatencyMs{0};
    std::atomic<double> m_jitterMs{0};
    std::atomic<qint64> m_droppedFrames{0};
    std::atomic<qint64> m_totalFrames{0};

    // History ring buffers (mutex-protected -- read from QML/UI thread)
    static constexpr int kHistoryLength = 300; // 30s at 10Hz
    mutable std::mutex   m_historyMutex;
    QList<double>        m_bitrateHistory;
    QList<double>        m_latencyHistory;
    double               m_prevLatencyMs{0}; // for jitter calc

    QTimer* m_uiRefreshTimer{nullptr};
};

} // namespace aura
