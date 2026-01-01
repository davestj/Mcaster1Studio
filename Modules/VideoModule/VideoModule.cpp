#include "VideoModule.h"
#include "VideoWidget.h"
#include "AudioBuffer.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

VideoModule::VideoModule(QObject* parent)
    : IModule(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(1.0f);

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &VideoModule::onPlayerStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoModule::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &VideoModule::onPlayerError);
}

VideoModule::~VideoModule() {
    shutdown();
}

// ─── IModule ──────────────────────────────────────────────────────────────────

void VideoModule::initialize() {
    qInfo() << "[VideoModule] initialized.";
}

void VideoModule::shutdown() {
    if (m_player) {
        m_player->stop();
    }
}

QWidget* VideoModule::createWidget(QWidget* parent) {
    auto* w = new VideoWidget(this, parent);
    return w;
}

// ─── Playback controls ────────────────────────────────────────────────────────

void VideoModule::loadFile(const QString& path) {
    // If not in playlist, add it as a one-shot play
    m_player->setSource(QUrl::fromLocalFile(path));
    m_state = State::Idle;
    emit nowPlayingChanged(path);
}

void VideoModule::play() {
    if (m_playlist.isEmpty() && m_player->source().isEmpty())
        return;

    if (m_currentIndex < 0 && !m_playlist.isEmpty()) {
        m_currentIndex = 0;
        applyCurrentIndex();
    }

    m_player->play();
}

void VideoModule::pause() {
    if (m_state == State::Playing) {
        m_player->pause();
    } else if (m_state == State::Paused) {
        m_player->play();
    }
}

void VideoModule::stop() {
    m_player->stop();
    m_state = State::Idle;
    emit stateChanged(m_state);
}

void VideoModule::next() {
    if (m_playlist.isEmpty()) return;
    int nextIdx = m_currentIndex + 1;
    if (nextIdx >= m_playlist.size())
        nextIdx = 0;
    playAtIndex(nextIdx);
}

void VideoModule::previous() {
    if (m_playlist.isEmpty()) return;
    int prevIdx = m_currentIndex - 1;
    if (prevIdx < 0)
        prevIdx = m_playlist.size() - 1;
    playAtIndex(prevIdx);
}

// ─── Playlist management ──────────────────────────────────────────────────────

void VideoModule::addToPlaylist(const QStringList& paths) {
    for (const QString& p : paths) {
        if (!m_playlist.contains(p)) {
            m_playlist.append(p);
        }
    }
    if (m_currentIndex < 0 && !m_playlist.isEmpty())
        m_currentIndex = 0;
    emit playlistChanged();
}

void VideoModule::removeFromPlaylist(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_playlist.removeAt(index);
    if (m_currentIndex >= m_playlist.size())
        m_currentIndex = m_playlist.size() - 1;
    emit playlistChanged();
}

void VideoModule::playAtIndex(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_currentIndex = index;
    applyCurrentIndex();
    m_player->play();
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void VideoModule::applyCurrentIndex() {
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) return;
    const QString& path = m_playlist[m_currentIndex];
    m_player->setSource(QUrl::fromLocalFile(path));
    emit nowPlayingChanged(path);
}

// ─── Qt media player slots ────────────────────────────────────────────────────

void VideoModule::onPlayerStateChanged(QMediaPlayer::PlaybackState state) {
    switch (state) {
    case QMediaPlayer::PlayingState:
        m_state = State::Playing;
        emit statusChanged("Playing");
        break;
    case QMediaPlayer::PausedState:
        m_state = State::Paused;
        emit statusChanged("Paused");
        break;
    case QMediaPlayer::StoppedState:
        m_state = State::Idle;
        emit statusChanged("Stopped");
        break;
    }
    emit stateChanged(m_state);
}

void VideoModule::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        // Auto-advance to next item in playlist
        if (!m_playlist.isEmpty()) {
            next();
        }
    }
}

void VideoModule::onPlayerError(QMediaPlayer::Error /*error*/, const QString& errorString) {
    qWarning() << "[VideoModule] Playback error:" << errorString;
    emit moduleError(errorString);
    emit statusChanged("Error: " + errorString);
}

// ─── State persistence ────────────────────────────────────────────────────────

void VideoModule::saveState(QSettings& s) {
    // Save playlist as a JSON array
    QJsonArray arr;
    for (const QString& p : m_playlist)
        arr.append(p);
    QJsonDocument doc(arr);
    s.setValue("video/playlist",     doc.toJson(QJsonDocument::Compact));
    s.setValue("video/currentIndex", m_currentIndex);
    s.setValue("video/volume",       m_audioOutput->volume());
}

void VideoModule::loadState(QSettings& s) {
    const double volume = s.value("video/volume", 1.0).toDouble();
    m_audioOutput->setVolume(static_cast<float>(volume));

    const QByteArray json = s.value("video/playlist").toByteArray();
    if (!json.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(json);
        if (doc.isArray()) {
            m_playlist.clear();
            for (const auto& v : doc.array())
                m_playlist.append(v.toString());
        }
    }

    m_currentIndex = s.value("video/currentIndex", -1).toInt();
    if (m_currentIndex >= m_playlist.size())
        m_currentIndex = m_playlist.isEmpty() ? -1 : 0;

    emit playlistChanged();
}

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────

static Mcaster1PluginInfo s_videoInfo{
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.video",
    "Video",
    "1.0.0",
    "entertainment,social,church",
    "module",
    "Mcaster1",
    "Video file player with playlist — mp4, mkv, avi, mov, wmv, webm"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_video_plugin_info() {
    return &s_videoInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_video_create(IModuleHost*) {
    return new M1::VideoModule();
}
MCASTER1_PLUGIN_API void mcaster1_video_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
