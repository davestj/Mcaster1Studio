#pragma once
#include <QRunnable>
#include <functional>
#include <atomic>

namespace M1 {

/// QRunnable wrapper that executes a lambda and increments a completion counter.
/// Used by SurfaceThreadPool to track task metrics without requiring
/// every module to manually manage counters.
class PooledRunnable : public QRunnable {
public:
    /// @param task       The work to execute in the pool thread.
    /// @param completed  Atomic counter incremented when the task finishes.
    PooledRunnable(std::function<void()> task, std::atomic<qint64>& completed)
        : m_task(std::move(task))
        , m_completed(completed)
    {
        setAutoDelete(true);
    }

    void run() override {
        if (m_task)
            m_task();
        m_completed.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::function<void()> m_task;
    std::atomic<qint64>&  m_completed;
};

} // namespace M1
