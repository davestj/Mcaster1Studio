#include "SurfaceThreadPool.h"
#include "PooledRunnable.h"
#include <QtConcurrent/QtConcurrent>
#include <QDebug>
#include <algorithm>

namespace M1 {

SurfaceThreadPool::SurfaceThreadPool(const QString& surfaceName, int maxThreads,
                                       QObject* parent)
    : QObject(parent)
    , m_surfaceName(surfaceName)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(std::max(1, maxThreads));
    qInfo() << "[ThreadPool]" << surfaceName << "created with"
            << m_pool->maxThreadCount() << "threads";
}

SurfaceThreadPool::~SurfaceThreadPool()
{
    qInfo() << "[ThreadPool]" << m_surfaceName << "shutting down —"
            << m_tasksCompleted.load() << "tasks completed,"
            << pendingTaskCount() << "pending";
    m_pool->waitForDone(5000);
}

void SurfaceThreadPool::submitTask(std::function<void()> task, int priority)
{
    m_tasksSubmitted.fetch_add(1, std::memory_order_relaxed);

    // Update peak queue depth
    int pending = pendingTaskCount();
    int prevPeak = m_peakQueueDepth.load(std::memory_order_relaxed);
    while (pending > prevPeak &&
           !m_peakQueueDepth.compare_exchange_weak(prevPeak, pending,
               std::memory_order_relaxed)) {
        // CAS loop
    }

    auto* runnable = new PooledRunnable(std::move(task), m_tasksCompleted);
    m_pool->start(runnable, priority);
}

int SurfaceThreadPool::pendingTaskCount() const
{
    qint64 sub = m_tasksSubmitted.load(std::memory_order_relaxed);
    qint64 com = m_tasksCompleted.load(std::memory_order_relaxed);
    return static_cast<int>(std::max<qint64>(0, sub - com));
}

bool SurfaceThreadPool::waitForDone(int timeoutMs)
{
    return m_pool->waitForDone(timeoutMs);
}

} // namespace M1
