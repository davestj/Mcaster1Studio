/// test_threadpool.cpp — Thread pool and CPU affinity integration tests.
///
/// Tests:
///   1. SurfaceThreadPool — creation, task submission, metrics
///   2. ThreadPoolManager — pool lifecycle, budget computation, affinity
///   3. IThreadPoolAware — mixin injection pattern
///
/// Build:
///   cmake --build build --config Debug --target TestThreadPool
///
/// Run:
///   build/bin/Debug/TestThreadPool.exe

#include <QCoreApplication>
#include <QThread>
#include <QElapsedTimer>
#include <cstdio>
#include <atomic>

#include "SurfaceThreadPool.h"
#include "ThreadPoolManager.h"
#include "IThreadPoolAware.h"
#include "IModule.h"
#include "ISurface.h"

static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

static void check(bool ok, const char* name, const QString& detail = {}) {
    if (ok) {
        fprintf(stdout, "  [PASS] %s\n", name);
        ++g_pass;
    } else {
        fprintf(stdout, "  [FAIL] %s -- %s\n", name, detail.toUtf8().constData());
        ++g_fail;
    }
    fflush(stdout);
}

static void skip(const char* name, const QString& reason) {
    fprintf(stdout, "  [SKIP] %s -- %s\n", name, reason.toUtf8().constData());
    ++g_skip;
    fflush(stdout);
}

static void header(const char* msg) {
    fprintf(stdout, "\n%s\n", msg);
    fflush(stdout);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: SurfaceThreadPool basics
// ═══════════════════════════════════════════════════════════════════════════
static void testPoolBasics() {
    header("=== Test 1: SurfaceThreadPool Basics ===");

    M1::SurfaceThreadPool pool("TestSurface", 4);

    check(pool.surfaceName() == "TestSurface", "Pool surface name");
    check(pool.maxThreadCount() == 4, "Pool max threads = 4");
    check(pool.activeThreadCount() == 0, "Pool initially 0 active");
    check(pool.tasksSubmitted() == 0, "Pool initially 0 submitted");
    check(pool.tasksCompleted() == 0, "Pool initially 0 completed");
    check(pool.pendingTaskCount() == 0, "Pool initially 0 pending");
    check(pool.pool() != nullptr, "Underlying QThreadPool not null");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Task submission and metrics
// ═══════════════════════════════════════════════════════════════════════════
static void testTaskSubmission() {
    header("=== Test 2: Task Submission + Metrics ===");

    M1::SurfaceThreadPool pool("TaskTest", 4);

    std::atomic<int> counter{0};
    const int taskCount = 100;

    // Submit 100 tasks
    for (int i = 0; i < taskCount; ++i) {
        pool.submitTask([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    check(pool.tasksSubmitted() == taskCount,
          "100 tasks submitted",
          QString("got %1").arg(pool.tasksSubmitted()));

    // Wait for all to complete
    bool done = pool.waitForDone(10000);
    check(done, "All tasks completed within 10s timeout");
    check(counter.load() == taskCount,
          "Counter reached 100",
          QString("got %1").arg(counter.load()));
    check(pool.tasksCompleted() == taskCount,
          "tasksCompleted == 100",
          QString("got %1").arg(pool.tasksCompleted()));
    check(pool.pendingTaskCount() == 0,
          "Pending count 0 after completion");
    check(pool.peakQueueDepth() > 0,
          "Peak queue depth > 0 (tasks were queued)",
          QString("peak=%1").arg(pool.peakQueueDepth()));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: runAsync with QFuture
// ═══════════════════════════════════════════════════════════════════════════
static void testRunAsync() {
    header("=== Test 3: runAsync + QFuture ===");

    M1::SurfaceThreadPool pool("AsyncTest", 2);

    std::atomic<bool> ran{false};
    auto future = pool.runAsync([&ran]() {
        ran.store(true, std::memory_order_relaxed);
    });

    future.waitForFinished();
    check(ran.load(), "runAsync lambda executed");
    check(pool.tasksCompleted() == 1, "tasksCompleted == 1 after runAsync");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: ThreadPoolManager singleton
// ═══════════════════════════════════════════════════════════════════════════
static void testManagerSingleton() {
    header("=== Test 4: ThreadPoolManager Singleton ===");

    auto& mgr = M1::ThreadPoolManager::instance();
    check(mgr.availableCores() > 0,
          "Available cores > 0",
          QString("%1 cores").arg(mgr.availableCores()));

    // Affinity enabled if >= 4 cores
    if (mgr.availableCores() >= 4) {
        check(mgr.affinityEnabled(), "Affinity enabled (>= 4 cores)");
    } else {
        check(!mgr.affinityEnabled(), "Affinity disabled (< 4 cores)");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Pool lifecycle (create + destroy)
// ═══════════════════════════════════════════════════════════════════════════
static void testPoolLifecycle() {
    header("=== Test 5: Pool Lifecycle ===");

    auto& mgr = M1::ThreadPoolManager::instance();

    // Create pool for a test surface
    auto* pool = mgr.createPoolForSurface("LifecycleTest",
                                           static_cast<int>(M1::SurfaceType::DJ));
    check(pool != nullptr, "createPoolForSurface returns non-null");
    check(pool->maxThreadCount() >= 2,
          "Pool has >= 2 threads",
          QString("%1 threads").arg(pool->maxThreadCount()));

    // Verify lookup
    auto* found = mgr.poolForSurface("LifecycleTest");
    check(found == pool, "poolForSurface finds the pool");

    // Verify null for non-existent
    check(mgr.poolForSurface("NonExistent") == nullptr, "Non-existent pool returns null");

    // Metrics
    auto metrics = mgr.allMetrics();
    bool foundInMetrics = false;
    for (const auto& m : metrics) {
        if (m.surfaceName == "LifecycleTest") {
            foundInMetrics = true;
            check(m.maxThreads >= 2, "Metrics: maxThreads >= 2");
        }
    }
    check(foundInMetrics, "Pool appears in allMetrics()");

    // Destroy
    mgr.destroyPoolForSurface("LifecycleTest");
    check(mgr.poolForSurface("LifecycleTest") == nullptr,
          "Pool removed after destroy");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Multiple surface pools
// ═══════════════════════════════════════════════════════════════════════════
static void testMultiplePools() {
    header("=== Test 6: Multiple Surface Pools ===");

    auto& mgr = M1::ThreadPoolManager::instance();

    auto* p1 = mgr.createPoolForSurface("Multi-Alpha",
                                         static_cast<int>(M1::SurfaceType::Alpha));
    auto* p2 = mgr.createPoolForSurface("Multi-Church",
                                         static_cast<int>(M1::SurfaceType::Church));
    auto* p3 = mgr.createPoolForSurface("Multi-Podcast",
                                         static_cast<int>(M1::SurfaceType::Podcast));

    check(p1 != nullptr && p2 != nullptr && p3 != nullptr, "3 pools created");
    check(p1 != p2 && p2 != p3, "All pools are distinct");

    // All pools should have >= 2 threads; Church/Podcast get +1 bonus
    // Note: Alpha was created first (gets bigger budget), then Church splits it
    check(p1->maxThreadCount() >= 2, "Alpha pool >= 2 threads",
          QString("%1 threads").arg(p1->maxThreadCount()));
    check(p2->maxThreadCount() >= 2, "Church pool >= 2 threads",
          QString("%1 threads").arg(p2->maxThreadCount()));
    check(p3->maxThreadCount() >= 2, "Podcast pool >= 2 threads",
          QString("%1 threads").arg(p3->maxThreadCount()));

    // Submit work to each
    std::atomic<int> c1{0}, c2{0}, c3{0};
    p1->submitTask([&c1]() { c1++; });
    p2->submitTask([&c2]() { c2++; });
    p3->submitTask([&c3]() { c3++; });

    p1->waitForDone(5000);
    p2->waitForDone(5000);
    p3->waitForDone(5000);

    check(c1 == 1 && c2 == 1 && c3 == 1, "All 3 pools executed tasks");

    // Total active threads
    check(mgr.totalActiveThreads() >= 0, "totalActiveThreads >= 0");

    // Cleanup
    mgr.destroyPoolForSurface("Multi-Alpha");
    mgr.destroyPoolForSurface("Multi-Church");
    mgr.destroyPoolForSurface("Multi-Podcast");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: Encoder core assignment
// ═══════════════════════════════════════════════════════════════════════════
static void testEncoderCoreAssignment() {
    header("=== Test 7: Encoder Core Assignment ===");

    auto& mgr = M1::ThreadPoolManager::instance();
    mgr.resetEncoderCoreAssignments();

    if (!mgr.affinityEnabled()) {
        skip("Encoder core assignment", "Affinity disabled (< 4 cores)");
        return;
    }

    int c1 = mgr.assignEncoderCore();
    int c2 = mgr.assignEncoderCore();
    int c3 = mgr.assignEncoderCore();
    int c4 = mgr.assignEncoderCore(); // wraps around

    check(c1 == 1, "First encoder core = 1", QString("got %1").arg(c1));
    check(c2 == 2, "Second encoder core = 2", QString("got %1").arg(c2));
    check(c3 == 3, "Third encoder core = 3", QString("got %1").arg(c3));
    check(c4 == 1, "Fourth wraps to core 1", QString("got %1").arg(c4));

    mgr.resetEncoderCoreAssignments();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 8: Pool thread isolation (tasks run off GUI thread)
// ═══════════════════════════════════════════════════════════════════════════
static void testThreadIsolation() {
    header("=== Test 8: Thread Isolation ===");

    M1::SurfaceThreadPool pool("IsolationTest", 2);

    QThread* mainThread = QThread::currentThread();
    std::atomic<QThread*> taskThread{nullptr};

    pool.submitTask([&taskThread]() {
        taskThread.store(QThread::currentThread(), std::memory_order_relaxed);
    });

    pool.waitForDone(5000);

    QThread* worker = taskThread.load();
    check(worker != nullptr, "Task captured thread pointer");
    check(worker != mainThread,
          "Task ran on different thread (not GUI thread)",
          worker == mainThread ? "SAME thread!" : "different thread");
}

// ═══════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Thread Pool Integration Tests\n");
    fprintf(stdout, "  CPU cores: %d  |  Affinity: %s\n",
            QThread::idealThreadCount(),
            QThread::idealThreadCount() >= 4 ? "enabled" : "disabled");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testPoolBasics();
    testTaskSubmission();
    testRunAsync();
    testManagerSingleton();
    testPoolLifecycle();
    testMultiplePools();
    testEncoderCoreAssignment();
    testThreadIsolation();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
