#pragma once
#include <QObject>
#include <QThreadPool>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QString>
#include <atomic>
#include <functional>

namespace M1 {

/// Per-surface thread pool wrapper with metrics tracking.
///
/// Each SurfaceWidget owns one SurfaceThreadPool. Modules that implement
/// IThreadPoolAware receive a pointer to this pool and use submitTask()
/// or run() to offload heavy work to background threads.
///
/// Thread budget is managed by ThreadPoolManager based on available cores.
class SurfaceThreadPool : public QObject {
    Q_OBJECT

public:
    /// @param surfaceName  Name of the owning surface (for diagnostics).
    /// @param maxThreads   Maximum concurrent threads for this pool.
    /// @param parent       QObject parent (typically SurfaceWidget).
    explicit SurfaceThreadPool(const QString& surfaceName, int maxThreads,
                                QObject* parent = nullptr);
    ~SurfaceThreadPool() override;

    /// The owning surface's name.
    QString surfaceName() const { return m_surfaceName; }

    /// Access the underlying QThreadPool.
    QThreadPool* pool() const { return m_pool; }

    /// Submit a task to the pool. The lambda runs in a pool thread.
    /// Do NOT call Qt widget APIs from the lambda.
    void submitTask(std::function<void()> task, int priority = 0);

    /// Convenience: submit a void task and get a QFuture to track completion.
    /// Uses QtConcurrent::run on this pool's QThreadPool.
    QFuture<void> runAsync(std::function<void()> fn) {
        m_tasksSubmitted.fetch_add(1, std::memory_order_relaxed);
        return QtConcurrent::run(m_pool, [this, f = std::move(fn)]() {
            f();
            m_tasksCompleted.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // ── Metrics ──────────────────────────────────────────────────────

    /// Number of threads currently executing tasks.
    int activeThreadCount() const { return m_pool->activeThreadCount(); }

    /// Maximum threads this pool will use.
    int maxThreadCount() const { return m_pool->maxThreadCount(); }

    /// Tasks submitted but not yet completed.
    int pendingTaskCount() const;

    /// Total tasks submitted since pool creation.
    qint64 tasksSubmitted() const { return m_tasksSubmitted.load(std::memory_order_relaxed); }

    /// Total tasks completed since pool creation.
    qint64 tasksCompleted() const { return m_tasksCompleted.load(std::memory_order_relaxed); }

    /// Peak pending queue depth (high-water mark).
    int peakQueueDepth() const { return m_peakQueueDepth.load(std::memory_order_relaxed); }

    /// Wait for all pending tasks to complete (with timeout in ms).
    /// Returns true if all tasks finished within the timeout.
    bool waitForDone(int timeoutMs = 5000);

private:
    QString      m_surfaceName;
    QThreadPool* m_pool;

    std::atomic<qint64> m_tasksSubmitted{0};
    std::atomic<qint64> m_tasksCompleted{0};
    std::atomic<int>    m_peakQueueDepth{0};
};

} // namespace M1
