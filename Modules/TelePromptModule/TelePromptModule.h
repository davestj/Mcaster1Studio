#pragma once
/// @file   TelePromptModule.h
/// @path   Modules/TelePromptModule/TelePromptModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TelePrompt — Scrolling Script Teleprompter Module
/// @purpose Provides a teleprompter display for the pastor, worship leader, or
///          announcer. We handle script import, smooth scrolling with adjustable
///          speed, mirror mode, section markers, and remote scroll control.
/// @reason  Pastor-facing production tool — essential for prepared sermon delivery
///          and scripted announcements.
/// @changelog
///   2026-03-09  Initial implementation — script editor, scroll engine, mirror mode

#include "IModule.h"
#include <QList>
#include <QColor>

class QTimer;

namespace M1 {

class GraphicsEngineModule;

// ─── Script section marker — a named jump point in the script ───────────────
struct ScriptMarker {
    QString name;        ///< "Introduction", "Point 1", "Closing"
    int     charOffset;  ///< Character offset in the script text
};

// ─── TelePromptModule ───────────────────────────────────────────────────────
class TelePromptModule : public IModule {
    Q_OBJECT

public:
    explicit TelePromptModule(QObject* parent = nullptr);
    ~TelePromptModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.teleprompt"; }
    QString displayName() const override { return "TelePrompter"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {500, 400}; }
    QSize minimumModuleSize() const override { return {340, 260}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Graphics engine binding ─────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }

    // ── Script management ───────────────────────────────────────────────────
    void setScript(const QString& text);
    QString script() const { return m_script; }

    void addMarker(const QString& name, int charOffset);
    void removeMarker(int index);
    QList<ScriptMarker> markers() const { return m_markers; }

    // ── Scroll control ──────────────────────────────────────────────────────
    void startScroll();
    void stopScroll();
    void pauseScroll();
    void resumeScroll();
    bool isScrolling() const { return m_scrolling; }

    void setScrollSpeed(double pixelsPerSecond);
    double scrollSpeed() const { return m_scrollSpeed; }

    void setScrollPosition(double position);
    double scrollPosition() const { return m_scrollPosition; }

    void jumpToMarker(int markerIndex);

    // ── Display settings ────────────────────────────────────────────────────
    void setFontSize(int sizePt);
    int fontSize() const { return m_fontSize; }

    void setMirrorMode(bool mirror);
    bool mirrorMode() const { return m_mirrorMode; }

    void setTextColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    QColor textColor() const { return m_textColor; }
    QColor backgroundColor() const { return m_bgColor; }

signals:
    void scriptChanged();
    void scrollStateChanged(bool scrolling);
    void scrollPositionChanged(double position);
    void scrollSpeedChanged(double speed);
    void markerReached(int markerIndex);

private slots:
    void onScrollTick();

private:
    GraphicsEngineModule* m_graphics = nullptr;
    QTimer*  m_scrollTimer = nullptr;

    QString  m_script;
    QList<ScriptMarker> m_markers;

    // Scroll state
    bool     m_scrolling     = false;
    bool     m_paused        = false;
    double   m_scrollSpeed   = 60.0;     ///< Pixels per second
    double   m_scrollPosition = 0.0;     ///< Current Y offset in pixels

    // Display settings
    int      m_fontSize       = 32;
    bool     m_mirrorMode     = false;
    QColor   m_textColor      = QColor(255, 255, 255);
    QColor   m_bgColor        = QColor(0, 0, 0);
};

} // namespace M1
