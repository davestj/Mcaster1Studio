#pragma once
/// @file   SwitchCasterModule.h
/// @path   Modules/SwitchCasterModule/SwitchCasterModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-SwitchCaster — Live Production Switcher Module
/// @purpose Provides a Program/Preview bus switcher for church services,
///          allowing operators to select visual sources (lyrics, scripture,
///          announcements, media, video, logo, blank) and transition between
///          them with cut, dissolve, or fade-to-black transitions.
/// @reason  Church services need smooth visual transitions between worship
///          lyrics, scripture readings, sermon points, announcements, and
///          media playback — similar to vMix/OBS but integrated with all
///          church-specific content modules.
/// @changelog
///   2026-03-09  Initial implementation — Program/Preview, transitions, lower third

#include "IModule.h"
#include <QImage>
#include <atomic>

class QTimer;

namespace M1 {

class GraphicsEngineModule;
class LyricsCasterModule;
class ScriptureCasterModule;
class AnnounceCasterModule;
class MediaCasterModule;
class VideoModule;

// ─── SourceType — available visual sources ────────────────────────────────────
enum class SourceType {
    None,
    Lyrics,
    Scripture,
    Announce,
    MediaCaster,
    Video,
    Camera,
    Logo,
    Blank
};

// ─── TransitionType ───────────────────────────────────────────────────────────
enum class TransitionType {
    Cut,
    Dissolve,
    FadeToBlack
};

// ─── SwitchCasterModule ───────────────────────────────────────────────────────
class SwitchCasterModule : public IModule {
    Q_OBJECT

public:
    explicit SwitchCasterModule(QObject* parent = nullptr);
    ~SwitchCasterModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.switchcaster"; }
    QString displayName() const override { return "Production Switcher"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {700, 450}; }
    QSize minimumModuleSize() const override { return {500, 350}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Module bindings ─────────────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }
    void setLyricsCaster(LyricsCasterModule* lyrics)     { m_lyrics = lyrics; }
    void setScriptureCaster(ScriptureCasterModule* scr)   { m_scripture = scr; }
    void setAnnounceCaster(AnnounceCasterModule* ann)     { m_announce = ann; }
    void setMediaCaster(MediaCasterModule* media)         { m_mediaCaster = media; }
    void setVideoModule(VideoModule* video)               { m_video = video; }

    // ── Program / Preview bus ───────────────────────────────────────────────
    void setPreview(SourceType src);
    void setProgram(SourceType src);
    SourceType programSource() const { return m_programSource; }
    SourceType previewSource() const { return m_previewSource; }

    // ── Transitions ─────────────────────────────────────────────────────────
    void cut();                         ///< Preview → Program instantly
    void dissolve(int durationMs = 1000);
    void fadeToBlack();
    void fadeFromBlack();
    bool isTransitioning() const { return m_transitioning; }
    float transitionProgress() const;   ///< 0.0 → 1.0 during transition

    // ── Lower third overlay ─────────────────────────────────────────────────
    void showLowerThird(const QString& name, const QString& title = {});
    void hideLowerThird();
    bool isLowerThirdVisible() const { return m_lowerThirdVisible; }

    // ── Frame access ────────────────────────────────────────────────────────
    QImage programFrame() const;
    QImage previewFrame() const;

    // ── Source name utility ─────────────────────────────────────────────────
    static QString sourceTypeName(SourceType src);

signals:
    void programChanged(int sourceType);
    void previewChanged(int sourceType);
    void transitionStarted(int transType);
    void transitionFinished();
    void lowerThirdChanged(bool visible);
    void frameUpdated();

private slots:
    void onRefresh();
    void onTransitionTick();

private:
    // Bound modules
    GraphicsEngineModule*  m_graphics    = nullptr;
    LyricsCasterModule*    m_lyrics      = nullptr;
    ScriptureCasterModule* m_scripture   = nullptr;
    AnnounceCasterModule*  m_announce    = nullptr;
    MediaCasterModule*     m_mediaCaster = nullptr;
    VideoModule*           m_video       = nullptr;

    // Bus state
    SourceType m_programSource  = SourceType::Blank;
    SourceType m_previewSource  = SourceType::None;

    // Cached frames
    QImage m_programImg;
    QImage m_previewImg;

    // Transition state
    bool           m_transitioning     = false;
    TransitionType m_transType         = TransitionType::Cut;
    SourceType     m_transFromSource   = SourceType::Blank;
    SourceType     m_transToSource     = SourceType::Blank;
    int            m_transDurationMs   = 0;
    int            m_transElapsedMs    = 0;
    QImage         m_transFromFrame;
    QImage         m_transToFrame;
    QTimer*        m_transTimer        = nullptr;

    // Lower third
    bool    m_lowerThirdVisible = false;
    QString m_lowerThirdName;
    QString m_lowerThirdTitle;

    // Refresh timer
    QTimer* m_refreshTimer = nullptr;

    // Helpers
    QImage frameForSource(SourceType src) const;
    QImage blendFrames(const QImage& from, const QImage& to, float progress) const;
};

} // namespace M1
