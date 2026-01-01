#pragma once
/// @file   MediaCasterModule.h
/// @path   Modules/MediaCasterModule/MediaCasterModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-MediaCaster — Video and Media Playback Module
/// @purpose Provides video and audio media playback for church services.
///          We handle file-based video playback (MP4, MOV, AVI, WebM), audio
///          backing tracks, cue points, preview mode, and volume control
///          independent from the main audio mix.
/// @reason  Church services use pre-produced video content — sermon bumpers,
///          offering videos, countdown timers, worship backgrounds, and
///          special music tracks.
/// @changelog
///   2026-03-09  Initial implementation — video player, cue points, preview

#include "IModule.h"
#include <QList>
#include <QUrl>

class QTimer;

namespace M1 {

class GraphicsEngineModule;

// ─── Cue point within a service media file ──────────────────────────────────
/// We prefix with "Service" to avoid collision with M1::MediaItem from the
/// media library (Core/include/MediaItem.h).
struct ServiceCuePoint {
    QString label;        ///< "Intro End", "Chorus Start", etc.
    qint64  positionMs;   ///< Millisecond offset in the media file
};

// ─── Service media item — video/audio content for church services ───────────
struct ServiceMediaItem {
    QString  title;       ///< Display name for the operator
    QUrl     filePath;    ///< Local file path or URL
    qint64   durationMs = 0;  ///< Duration in milliseconds (0 if unknown)
    QList<ServiceCuePoint> cuePoints;
    bool     isVideo  = true;   ///< false for audio-only files
    bool     looping  = false;  ///< Loop playback for backgrounds
};

// ─── MediaCasterModule ──────────────────────────────────────────────────────
class MediaCasterModule : public IModule {
    Q_OBJECT

public:
    explicit MediaCasterModule(QObject* parent = nullptr);
    ~MediaCasterModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.mediacaster"; }
    QString displayName() const override { return "Media Caster"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {640, 480}; }
    QSize minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Graphics engine binding ─────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }

    // ── Playlist management ─────────────────────────────────────────────────
    void addMediaItem(const ServiceMediaItem& item);
    void removeMediaItem(int index);
    void clearPlaylist();
    int  mediaItemCount() const { return m_playlist.size(); }
    ServiceMediaItem mediaItem(int index) const;
    QList<ServiceMediaItem> playlist() const { return m_playlist; }

    // ── Playback control ────────────────────────────────────────────────────
    void loadItem(int index);
    void play();
    void pause();
    void stop();
    void seekTo(qint64 positionMs);
    void jumpToCue(int cueIndex);

    bool isPlaying() const { return m_playing; }
    bool isPaused()  const { return m_paused; }
    qint64 position()  const { return m_positionMs; }
    qint64 duration()  const { return m_durationMs; }
    int    currentIndex() const { return m_currentIndex; }

    // ── Volume control (independent from main mix) ──────────────────────────
    void setVolume(double volume);   ///< 0.0–1.0
    double volume() const { return m_volume; }

    // ── Loop mode ───────────────────────────────────────────────────────────
    void setLooping(bool loop);
    bool isLooping() const { return m_looping; }

    // ── Preview mode ────────────────────────────────────────────────────────
    void setPreviewMode(bool preview);
    bool previewMode() const { return m_previewMode; }

signals:
    void playbackStateChanged(bool playing);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void mediaLoaded(int index);
    void playlistChanged();
    void cuePointReached(int cueIndex, const QString& label);
    void mediaFinished();

private slots:
    void onPositionTick();

private:
    GraphicsEngineModule* m_graphics = nullptr;
    QTimer*  m_positionTimer = nullptr;

    QList<ServiceMediaItem> m_playlist;
    int      m_currentIndex = -1;

    // Playback state
    bool     m_playing    = false;
    bool     m_paused     = false;
    qint64   m_positionMs = 0;
    qint64   m_durationMs = 0;
    double   m_volume     = 1.0;
    bool     m_looping    = false;
    bool     m_previewMode = false;
};

} // namespace M1
