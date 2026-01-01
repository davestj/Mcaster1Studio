#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QList>
#include <QString>
#include <QMediaPlayer>
#include <QAudioOutput>

class VideoWidget;

namespace M1 {

/// VideoModule — Phase 9 video playback and playlist management.
///
/// Provides a Qt6 QMediaPlayer-based video player with playlist support.
/// Supports mp4, mkv, avi, mov, wmv, webm file formats.
///
/// RTMP streaming is a future feature stub (Phase 9b — FFmpeg integration).
///
/// onAudioBlock() is a no-op — video audio is handled by QMediaPlayer / QAudioOutput.
class VideoModule : public IModule {
    Q_OBJECT

public:
    enum class State { Idle, Playing, Paused };

    explicit VideoModule(QObject* parent = nullptr);
    ~VideoModule() override;

    // ── IModule ──────────────────────────────────────────────────────────
    QString  moduleId()      const override { return "com.mcaster1.video"; }
    QString  displayName()   const override { return "Video"; }
    QSize    preferredSize() const override { return {800, 450}; }

    QWidget* createWidget(QWidget* parent) override;

    void initialize() override;
    void shutdown()   override;

    // onAudioBlock is a no-op — video audio is routed by Qt multimedia
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Playback controls ────────────────────────────────────────────────
    void loadFile(const QString& path);
    void play();
    void pause();
    void stop();
    void next();
    void previous();

    // ── Playlist access ──────────────────────────────────────────────────
    const QList<QString>& playlist() const { return m_playlist; }
    int                   currentIndex() const { return m_currentIndex; }
    State                 playbackState() const { return m_state; }

    QMediaPlayer* mediaPlayer() { return m_player; }
    QAudioOutput* audioOutput() { return m_audioOutput; }

signals:
    void nowPlayingChanged(const QString& path);
    void playlistChanged();
    void stateChanged(VideoModule::State state);

public slots:
    void addToPlaylist(const QStringList& paths);
    void removeFromPlaylist(int index);
    void playAtIndex(int index);

private slots:
    void onPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString& errorString);

private:
    void applyCurrentIndex();

    QMediaPlayer*   m_player      = nullptr;
    QAudioOutput*   m_audioOutput = nullptr;
    QList<QString>  m_playlist;
    int             m_currentIndex = -1;
    State           m_state        = State::Idle;
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_video_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_video_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_video_destroy(M1::IModule*);
}
