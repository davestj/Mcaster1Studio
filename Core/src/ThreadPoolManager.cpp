#include "ThreadPoolManager.h"
#include "SurfaceThreadPool.h"
#include "ISurface.h"
#include <QDebug>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace M1 {

// ─── Singleton ───────────────────────────────────────────────────────────────

ThreadPoolManager& ThreadPoolManager::instance()
{
    static ThreadPoolManager s;
    return s;
}

ThreadPoolManager::ThreadPoolManager()
    : m_availableCores(QThread::idealThreadCount())
{
    qInfo() << "[ThreadPoolManager] Initialized —" << m_availableCores << "CPU cores available"
            << "— affinity" << (affinityEnabled() ? "enabled" : "disabled");
}

ThreadPoolManager::~ThreadPoolManager()
{
    // Clean up any remaining pools
    for (auto* pool : m_pools)
        delete pool;
    m_pools.clear();
}

// ─── Surface lifecycle ──────────────────────────────────────────────────────

SurfaceThreadPool* ThreadPoolManager::createPoolForSurface(
    const QString& surfaceName, int surfaceTypeInt)
{
    // Don't create duplicate pools
    if (m_pools.contains(surfaceName)) {
        qWarning() << "[ThreadPoolManager] Pool already exists for" << surfaceName;
        return m_pools[surfaceName];
    }

    int threads = computeThreadBudget(surfaceTypeInt);
    auto* pool = new SurfaceThreadPool(surfaceName, threads);
    m_pools.insert(surfaceName, pool);

    emit poolCreated(surfaceName);
    return pool;
}

void ThreadPoolManager::destroyPoolForSurface(const QString& surfaceName)
{
    auto it = m_pools.find(surfaceName);
    if (it == m_pools.end()) return;

    auto* pool = it.value();
    m_pools.erase(it);

    // Wait for pending tasks before deletion
    pool->waitForDone(5000);
    delete pool;

    emit poolDestroyed(surfaceName);
    qInfo() << "[ThreadPoolManager] Pool destroyed for" << surfaceName;
}

SurfaceThreadPool* ThreadPoolManager::poolForSurface(const QString& surfaceName) const
{
    return m_pools.value(surfaceName, nullptr);
}

// ─── Thread budget ──────────────────────────────────────────────────────────

int ThreadPoolManager::computeThreadBudget(int surfaceTypeInt) const
{
    // Reserve cores:
    //   Core 0: PortAudio RT callback
    //   Cores 1-3: Encoder threads (up to 3)
    const int reservedForRT = 1;
    const int reservedForEncoders = std::min(kMaxEncoderCores,
                                             std::max(0, m_availableCores - 4));
    const int remaining = std::max(1, m_availableCores - reservedForRT - reservedForEncoders);

    // Divide remaining cores among active surface pools
    const int activePools = std::max(1, static_cast<int>(m_pools.size()) + 1); // +1 for the one being created
    int budget = std::max(2, remaining / activePools);

    // Church and Podcast surfaces get a bonus thread (heavy modules)
    auto type = static_cast<SurfaceType>(surfaceTypeInt);
    if (type == SurfaceType::Church || type == SurfaceType::Podcast) {
        budget += 1;
    }

    // Cap at remaining cores
    budget = std::min(budget, remaining);

    qInfo() << "[ThreadPoolManager] Budget for surface type" << surfaceTypeInt
            << ":" << budget << "threads"
            << "(cores:" << m_availableCores
            << "reserved:" << (reservedForRT + reservedForEncoders)
            << "pools:" << activePools << ")";

    return budget;
}

// ─── CPU affinity ───────────────────────────────────────────────────────────

void ThreadPoolManager::pinCurrentThreadToCore(int coreIndex)
{
#ifdef Q_OS_WIN
    if (coreIndex < 0) return;

    DWORD_PTR mask = 1ULL << coreIndex;
    HANDLE thread = GetCurrentThread();
    DWORD_PTR prev = SetThreadAffinityMask(thread, mask);
    if (prev == 0) {
        qWarning() << "[ThreadPoolManager] SetThreadAffinityMask failed for core"
                   << coreIndex << "error:" << GetLastError();
    } else {
        qInfo() << "[ThreadPoolManager] Pinned thread to core" << coreIndex;
    }
#else
    (void)coreIndex;
    // Linux/macOS: could use pthread_setaffinity_np / thread_policy_set
    // Deferred to platform-specific implementation
#endif
}

int ThreadPoolManager::assignEncoderCore()
{
    if (!affinityEnabled()) return -1;

    // Round-robin cores 1..kMaxEncoderCores
    int core = m_nextEncoderCore;
    m_nextEncoderCore = 1 + ((m_nextEncoderCore) % kMaxEncoderCores);

    return core;
}

void ThreadPoolManager::resetEncoderCoreAssignments()
{
    m_nextEncoderCore = 1;
}

// ─── Metrics ────────────────────────────────────────────────────────────────

QList<ThreadPoolManager::PoolMetrics> ThreadPoolManager::allMetrics() const
{
    QList<PoolMetrics> result;
    result.reserve(m_pools.size());

    for (auto it = m_pools.constBegin(); it != m_pools.constEnd(); ++it) {
        auto* pool = it.value();
        PoolMetrics m;
        m.surfaceName    = pool->surfaceName();
        m.activeThreads  = pool->activeThreadCount();
        m.maxThreads     = pool->maxThreadCount();
        m.pendingTasks   = pool->pendingTaskCount();
        m.tasksCompleted = pool->tasksCompleted();
        result.append(m);
    }

    return result;
}

int ThreadPoolManager::totalActiveThreads() const
{
    int total = 0;
    for (auto* pool : m_pools)
        total += pool->activeThreadCount();
    return total;
}

} // namespace M1
