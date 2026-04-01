#include "HealthModule.h"
#include "HealthWidget.h"
#include "AudioBuffer.h"
#include "EncoderModule.h"
#include "DeckModule.h"
#include "DeckAModule.h"
#include "DeckBModule.h"
#include "DeckPlayer.h"
#include <QSettings>
#include <QDateTime>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace M1 {

// --- Constructor / Destructor --------------------------------------------------

HealthModule::HealthModule(QObject* parent)
    : IModule(parent)
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(2000); // 2s
    connect(m_refreshTimer, &QTimer::timeout, this, &HealthModule::onRefreshTimer);
}

HealthModule::~HealthModule() {
    shutdown();
}

// --- IModule -------------------------------------------------------------------

void HealthModule::initialize() {
    // Initialize CPU baseline
    measureCpu();
    m_refreshTimer->start();
    qInfo() << "[HealthModule] initialized — polling every 2s.";
}

void HealthModule::shutdown() {
    m_refreshTimer->stop();
}

QWidget* HealthModule::createWidget(QWidget* parent) {
    return new HealthWidget(this, parent);
}

// --- Module references ---------------------------------------------------------

void HealthModule::setEncoderModule(EncoderModule* enc) { m_encoder = enc; }
void HealthModule::setDeckModule(DeckModule* deck)      { m_deck = deck; }
void HealthModule::setDeckAModule(DeckAModule* a)        { m_deckA = a; }
void HealthModule::setDeckBModule(DeckBModule* b)        { m_deckB = b; }

// --- Timer callback ------------------------------------------------------------

void HealthModule::onRefreshTimer() {
    collectSnapshot();
}

void HealthModule::collectSnapshot() {
    HealthSnapshot snap;
    snap.timestampMs = QDateTime::currentMSecsSinceEpoch();

    // CPU + Memory
    measureCpu();
    measureMemory();
    snap.cpuPercent  = m_lastSnapshot.cpuPercent;
    snap.memoryBytes = m_lastSnapshot.memoryBytes;
    snap.peakMemory  = m_lastSnapshot.peakMemory;

    // Encoder stats — use public API (slotCount / activeSlotCount)
    if (m_encoder) {
        snap.encoderTotal = m_encoder->slotCount();
        snap.encoderLive  = m_encoder->activeSlotCount();
    }

    // Deck status via DeckModule (dual-deck)
    if (m_deck) {
        auto* playerA = m_deck->deckA();
        auto* playerB = m_deck->deckB();
        if (playerA) {
            snap.deckAPlaying  = (playerA->state() == DeckPlayer::State::Playing);
            snap.deckATrack    = playerA->tagArtist().isEmpty()
                                     ? playerA->tagTitle()
                                     : playerA->tagArtist() + " - " + playerA->tagTitle();
            snap.deckAPosition = playerA->positionSeconds();
        }
        if (playerB) {
            snap.deckBPlaying  = (playerB->state() == DeckPlayer::State::Playing);
            snap.deckBTrack    = playerB->tagArtist().isEmpty()
                                     ? playerB->tagTitle()
                                     : playerB->tagArtist() + " - " + playerB->tagTitle();
            snap.deckBPosition = playerB->positionSeconds();
        }
    }

    // Standalone Deck A module (overrides DeckModule values if present)
    if (m_deckA) {
        auto* player = m_deckA->player();
        if (player) {
            snap.deckAPlaying  = (player->state() == DeckPlayer::State::Playing);
            snap.deckATrack    = player->tagArtist().isEmpty()
                                     ? player->tagTitle()
                                     : player->tagArtist() + " - " + player->tagTitle();
            snap.deckAPosition = player->positionSeconds();
        }
    }

    // Standalone Deck B module (overrides DeckModule values if present)
    if (m_deckB) {
        auto* player = m_deckB->player();
        if (player) {
            snap.deckBPlaying  = (player->state() == DeckPlayer::State::Playing);
            snap.deckBTrack    = player->tagArtist().isEmpty()
                                     ? player->tagTitle()
                                     : player->tagArtist() + " - " + player->tagTitle();
            snap.deckBPosition = player->positionSeconds();
        }
    }

    // Thread pool metrics (Golden Path)
    auto& tpm = ThreadPoolManager::instance();
    snap.availableCores    = tpm.availableCores();
    snap.affinityEnabled   = tpm.affinityEnabled();
    snap.totalWorkerThreads = tpm.totalActiveThreads();
    for (const auto& pm : tpm.allMetrics()) {
        PoolHealth ph;
        ph.surfaceName    = pm.surfaceName;
        ph.activeThreads  = pm.activeThreads;
        ph.maxThreads     = pm.maxThreads;
        ph.pendingTasks   = pm.pendingTasks;
        ph.tasksCompleted = pm.tasksCompleted;
        snap.threadPools.append(ph);
    }

    m_lastSnapshot = snap;

    // History ring buffer
    m_history.append(snap);
    while (m_history.size() > kHistorySize)
        m_history.removeFirst();

    emit snapshotUpdated(snap);
    emit statusChanged(QString("CPU: %1% | Mem: %2 MB")
        .arg(snap.cpuPercent, 0, 'f', 1)
        .arg(snap.memoryBytes / (1024 * 1024)));
}

// --- Windows CPU measurement ---------------------------------------------------

void HealthModule::measureCpu() {
#ifdef Q_OS_WIN
    FILETIME creation, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto toInt64 = [](const FILETIME& ft) -> qint64 {
            return (qint64(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        const qint64 k = toInt64(kernel);
        const qint64 u = toInt64(user);

        FILETIME wallFt;
        GetSystemTimeAsFileTime(&wallFt);
        const qint64 wall = toInt64(wallFt);

        if (m_lastCpuWall > 0) {
            const qint64 cpuDelta  = (k - m_lastCpuKernel) + (u - m_lastCpuUser);
            const qint64 wallDelta = wall - m_lastCpuWall;
            if (wallDelta > 0) {
                // Normalize by number of logical processors
                SYSTEM_INFO si;
                GetSystemInfo(&si);
                const double numCpus = si.dwNumberOfProcessors;
                m_lastSnapshot.cpuPercent =
                    100.0 * double(cpuDelta) / double(wallDelta) / numCpus;
            }
        }

        m_lastCpuKernel = k;
        m_lastCpuUser   = u;
        m_lastCpuWall   = wall;
    }
#endif
}

void HealthModule::measureMemory() {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_lastSnapshot.memoryBytes = static_cast<qint64>(pmc.WorkingSetSize);
        m_lastSnapshot.peakMemory  = static_cast<qint64>(pmc.PeakWorkingSetSize);
    }
#endif
}

// --- State persistence ---------------------------------------------------------

void HealthModule::saveState(QSettings& s) {
    s.setValue("health/metricsEnabled", false);
    s.setValue("health/metricsPort",    9100);
}

void HealthModule::loadState(QSettings& s) {
    (void)s; // Metrics server settings loaded separately by PreferencesDialog
}

} // namespace M1

// --- C ABI plugin exports ------------------------------------------------------

static Mcaster1PluginInfo s_healthInfo{
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.health",
    "System Health",
    "1.0.0",
    "*",
    "module",
    "Mcaster1",
    "Studio-wide system health monitor — CPU, memory, encoder, deck status"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_health_plugin_info() {
    return &s_healthInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_health_create(IModuleHost*) {
    return new M1::HealthModule();
}
MCASTER1_PLUGIN_API void mcaster1_health_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
