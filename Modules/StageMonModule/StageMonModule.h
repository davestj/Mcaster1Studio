#pragma once
/// @file   StageMonModule.h
/// @path   Modules/StageMonModule/StageMonModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-StageMon — Stage Confidence Monitor Module
/// @purpose Provides a confidence display output for the pastor and worship
///          team. We composite from LyricsCaster, ScriptureCaster, and
///          TimerClock to show current + next items, timers, and notes on
///          a separate stage-facing display.
/// @reason  The stage team needs more context than the congregation — they
///          need to see what's coming next, how much time is left, and
///          orientation cues for transitions.
/// @changelog
///   2026-03-09  Initial implementation — dual-mode display, compositing engine

#include "IModule.h"

class QTimer;

namespace M1 {

class GraphicsEngineModule;
class LyricsCasterModule;
class ScriptureCasterModule;
class TimerClockModule;

// ─── StageMonModule ─────────────────────────────────────────────────────────
class StageMonModule : public IModule {
    Q_OBJECT

public:
    /// Display mode for the stage monitor
    enum class MonitorMode {
        Worship,    ///< Lyrics view — current section, next section, arrangement position
        Sermon,     ///< Scripture view — current slide, next queue item, timers
        Combined    ///< Split view — both lyrics and scripture info
    };

    explicit StageMonModule(QObject* parent = nullptr);
    ~StageMonModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.stagemon"; }
    QString displayName() const override { return "Stage Monitor"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {600, 400}; }
    QSize minimumModuleSize() const override { return {400, 280}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Module bindings ─────────────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }
    void setLyricsCaster(LyricsCasterModule* lyrics)     { m_lyrics = lyrics; }
    void setScriptureCaster(ScriptureCasterModule* scripture) { m_scripture = scripture; }
    void setTimerClock(TimerClockModule* timer)           { m_timerClock = timer; }

    // ── Monitor mode control ────────────────────────────────────────────────
    void setMonitorMode(MonitorMode mode);
    MonitorMode monitorMode() const { return m_mode; }

    // ── Display settings ────────────────────────────────────────────────────
    void setFontSize(int sizePt);
    int  fontSize() const { return m_fontSize; }

    void setShowClock(bool show);
    bool showClock() const { return m_showClock; }

    void setShowTimer(bool show);
    bool showTimer() const { return m_showTimer; }

    void setShowNextPreview(bool show);
    bool showNextPreview() const { return m_showNextPreview; }

signals:
    void monitorModeChanged(int mode);
    void displayUpdated();

private slots:
    void onRefreshTick();

private:
    GraphicsEngineModule*   m_graphics    = nullptr;
    LyricsCasterModule*     m_lyrics      = nullptr;
    ScriptureCasterModule*  m_scripture   = nullptr;
    TimerClockModule*       m_timerClock  = nullptr;
    QTimer*                 m_refreshTimer = nullptr;

    MonitorMode m_mode          = MonitorMode::Worship;
    int         m_fontSize      = 28;
    bool        m_showClock     = true;
    bool        m_showTimer     = true;
    bool        m_showNextPreview = true;
};

} // namespace M1
