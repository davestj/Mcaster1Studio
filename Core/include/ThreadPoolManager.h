#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include <QThread>

namespace M1 {

class SurfaceThreadPool;
enum class SurfaceType;

/// Central coordinator for per-surface thread pools and CPU affinity.
///
/// Singleton owned by MainWindow. Responsibilities:
///   - Create/destroy SurfaceThreadPool instances tied to surface lifecycle
///   - Compute per-surface thread budget based on available CPU cores
///   - Manage CPU core affinity (Windows SetThreadAffinityMask)
///   - Report aggregate metrics for HealthModule
class ThreadPoolManager : public QObject {
    Q_OBJECT

public:
    static ThreadPoolManager& instance();

    // ── Surface lifecycle ────────────────────────────────────────────

    /// Create a thread pool for the given surface. Returns the pool.
    /// Thread count is computed based on available cores and surface type.
    SurfaceThreadPool* createPoolForSurface(const QString& surfaceName,
                                             int surfaceTypeInt);

    /// Destroy and clean up the pool for the given surface.
    void destroyPoolForSurface(const QString& surfaceName);

    /// Get the pool for a surface (nullptr if none exists).
    SurfaceThreadPool* poolForSurface(const QString& surfaceName) const;

    // ── CPU affinity ─────────────────────────────────────────────────

    /// Pin the calling thread to a specific CPU core (0-based).
    /// No-op on systems with < 4 cores.
    static void pinCurrentThreadToCore(int coreIndex);

    /// Pin an encoder thread to the next available encoder core.
    /// Returns the assigned core index, or -1 if affinity is disabled.
    int assignEncoderCore();

    /// Reset the encoder core round-robin counter.
    void resetEncoderCoreAssignments();

    // ── Metrics ──────────────────────────────────────────────────────

    struct PoolMetrics {
        QString surfaceName;
        int     activeThreads = 0;
        int     maxThreads    = 0;
        int     pendingTasks  = 0;
        qint64  tasksCompleted = 0;
    };

    /// Metrics for all active surface pools.
    QList<PoolMetrics> allMetrics() const;

    /// Total worker threads across all pools.
    int totalActiveThreads() const;

    /// Number of available CPU cores.
    int availableCores() const { return m_availableCores; }

    /// Whether CPU affinity pinning is enabled (requires >= 4 cores).
    bool affinityEnabled() const { return m_availableCores >= 4; }

signals:
    void poolCreated(const QString& surfaceName);
    void poolDestroyed(const QString& surfaceName);

private:
    ThreadPoolManager();
    ~ThreadPoolManager() override;

    int computeThreadBudget(int surfaceTypeInt) const;

    QMap<QString, SurfaceThreadPool*> m_pools;
    int m_availableCores;
    int m_nextEncoderCore = 1; // starts at core 1 (core 0 = RT)
    static constexpr int kMaxEncoderCores = 3;
};

} // namespace M1
