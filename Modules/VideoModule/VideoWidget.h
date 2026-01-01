#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QCheckBox>
#include <QSlider>
#include <QVideoWidget>
#include <QMediaPlayer>
#include "VideoModule.h"

/// VideoWidget — the primary UI surface for VideoModule.
///
/// Layout:
///   ┌──────────────────────────────┬──────────────────────┐
///   │  QVideoWidget (preview)      │  Playlist panel       │
///   │                              │  (collapsible)        │
///   │  [filename / pos / dur]      │  [Add Files] button   │
///   │  (overlay label at bottom)   │                       │
///   ├──────────────────────────────┴──────────────────────┤
///   │  [|<]  [Play/Pause]  [Stop]  [>|]   vol────  [RTMP] │
///   └──────────────────────────────────────────────────────┘
///
/// RTMP streaming is a future feature stub — placeholder checkbox with tooltip.
class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(M1::VideoModule* module, QWidget* parent = nullptr);

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onNextClicked();
    void onPreviousClicked();
    void onAddFilesClicked();
    void onPlaylistDoubleClicked(QListWidgetItem* item);
    void onNowPlayingChanged(const QString& path);
    void onPlaylistChanged();
    void onModuleStateChanged(M1::VideoModule::State state);
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onVolumeChanged(int value);
    void onTogglePlaylist();

private:
    void buildUi();
    void applyStyles();
    void updateInfoOverlay(qint64 posMs, qint64 durMs);
    static QString formatTime(qint64 ms);

    M1::VideoModule* m_module = nullptr;

    // Video area
    QVideoWidget*  m_videoWidget    = nullptr;
    QLabel*        m_infoOverlay    = nullptr;  ///< filename / position / duration

    // Playlist panel (right side, collapsible)
    QWidget*       m_playlistPanel  = nullptr;
    QListWidget*   m_playlistView   = nullptr;
    QPushButton*   m_addFilesBtn    = nullptr;
    QPushButton*   m_toggleListBtn  = nullptr;

    // Transport controls
    QPushButton*   m_prevBtn        = nullptr;
    QPushButton*   m_playPauseBtn   = nullptr;
    QPushButton*   m_stopBtn        = nullptr;
    QPushButton*   m_nextBtn        = nullptr;
    QSlider*       m_volumeSlider   = nullptr;

    // RTMP stub — Phase 9b
    // TODO: Phase 9b — connect to FFmpeg RTMP push pipeline
    QCheckBox*     m_rtmpCheckBox   = nullptr;

    // Layout splitter (video | playlist)
    QSplitter*     m_splitter       = nullptr;

    qint64         m_durationMs     = 0;
};
