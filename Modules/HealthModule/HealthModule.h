#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QTimer>
#include <QList>
#include <QString>

class HealthWidget;

namespace M1 {

class EncoderModule;
class DeckModule;
class DeckAModule;
class DeckBModule;

/// Snapshot of system health at a single poll instant.
struct HealthSnapshot {
    qint64  timestampMs   = 0;
    double  cpuPercent    = 0.0;
    qint64  memoryBytes   = 0;     // Working set (RSS)
    qint64  peakMemory    = 0;
    int     encoderTotal  = 0;
    int     encoderLive   = 0;
    bool    deckAPlaying  = false;
    bool    deckBPlaying  = false;
    QString deckATrack;
    QString deckBTrack;
    double  deckAPosition = 0.0;
    double  deckBPosition = 0.0;
};

/// HealthModule — Studio-wide system health monitor.
///
/// Collects CPU, memory, encoder slot status, and deck status every 2 seconds.
/// Provides a dockable widget with gauges, charts, and export capability.
/// Can serve Prometheus /metrics endpoint via MetricsServer.
class HealthModule : public IModule {
    Q_OBJECT

public:
    static constexpr int kHistorySize = 60; ///< 2s * 60 = 2 min history

    explicit HealthModule(QObject* parent = nullptr);
    ~HealthModule() override;

    // -- IModule ---------------------------------------------------------------
    QString  moduleId()      const override { return "com.mcaster1.health"; }
    QString  displayName()   const override { return "System Health"; }
    QSize    preferredSize() const override { return {500, 400}; }

    QWidget* createWidget(QWidget* parent) override;

    void initialize() override;
    void shutdown()   override;

    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // -- Module references (set by MainWindow) ---------------------------------
    void setEncoderModule(EncoderModule* enc);
    void setDeckModule(DeckModule* deck);
    void setDeckAModule(DeckAModule* a);
    void setDeckBModule(DeckBModule* b);

    // -- Data access -----------------------------------------------------------
    const HealthSnapshot& snapshot() const { return m_lastSnapshot; }
    const QList<HealthSnapshot>& history() const { return m_history; }

signals:
    void snapshotUpdated(const M1::HealthSnapshot& snap);

private slots:
    void onRefreshTimer();

private:
    void collectSnapshot();
    void measureCpu();
    void measureMemory();

    QTimer* m_refreshTimer = nullptr;
    HealthSnapshot m_lastSnapshot;
    QList<HealthSnapshot> m_history;

    // Module refs (non-owning)
    EncoderModule* m_encoder = nullptr;
    DeckModule*    m_deck    = nullptr;
    DeckAModule*   m_deckA   = nullptr;
    DeckBModule*   m_deckB   = nullptr;

    // CPU measurement state (Windows GetProcessTimes delta)
    qint64 m_lastCpuKernel = 0;
    qint64 m_lastCpuUser   = 0;
    qint64 m_lastCpuWall   = 0;
};

} // namespace M1

// --- C ABI plugin exports ------------------------------------------------------
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_health_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_health_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_health_destroy(M1::IModule*);
}
