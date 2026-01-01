/// @file   MediaCasterModule.cpp
/// @path   Modules/MediaCasterModule/MediaCasterModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-MediaCaster — Video and Media Playback Implementation
/// @purpose Implements video/audio playback engine, cue point navigation,
///          preview mode, and the operator control widget.
/// @reason  Church services rely on pre-produced video content for
///          countdowns, sermon bumpers, offering videos, and backgrounds.
/// @changelog
///   2026-03-09  Initial implementation

#include "MediaCasterModule.h"
#include "GraphicsEngineModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QListWidget>
#include <QCheckBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QPainter>
#include <QStyle>
#include <QSettings>
#include <QFileInfo>
#include <QMimeData>
#include <QApplication>
#include <QComboBox>
#include <QSplitter>
#include <QMessageBox>

namespace {

// ─── Time formatting helper ─────────────────────────────────────────────────
QString formatTime(qint64 ms) {
    if (ms < 0) ms = 0;
    int totalSec = static_cast<int>(ms / 1000);
    int hours    = totalSec / 3600;
    int minutes  = (totalSec % 3600) / 60;
    int seconds  = totalSec % 60;
    if (hours > 0)
        return QString("%1:%2:%3")
            .arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

// ─── Video Preview Widget — displays the media player output ────────────────
/// We use QVideoWidget for actual video rendering. When no video is loaded
/// we paint a placeholder dark rectangle with "No Media" text.
class VideoPreview : public QWidget {
public:
    explicit VideoPreview(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("MediaCasterPreview");
        setMinimumSize(320, 180);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);

        m_videoWidget = new QVideoWidget(this);
        m_videoWidget->setMinimumSize(320, 180);
        lay->addWidget(m_videoWidget);

        m_placeholder = new QLabel("No Media Loaded", this);
        m_placeholder->setAlignment(Qt::AlignCenter);
        m_placeholder->setObjectName("MediaCasterPlaceholder");
        m_placeholder->setStyleSheet(
            "background: #0a0a14; color: #666; font-size: 16px; font-style: italic;");
        lay->addWidget(m_placeholder);

        // We start with placeholder visible
        m_videoWidget->hide();
    }

    QVideoWidget* videoWidget() { return m_videoWidget; }

    void showVideo() {
        m_placeholder->hide();
        m_videoWidget->show();
    }
    void showPlaceholder() {
        m_videoWidget->hide();
        m_placeholder->show();
    }

private:
    QVideoWidget* m_videoWidget = nullptr;
    QLabel*       m_placeholder = nullptr;
};

// ─── PlaylistPanel — left-side media list with add/remove/reorder ───────────
class PlaylistPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistPanel(M1::MediaCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("MediaCasterPlaylist");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);

        auto* header = new QLabel("Media Playlist");
        header->setObjectName("MediaCasterHeader");
        lay->addWidget(header);

        m_list = new QListWidget;
        m_list->setObjectName("MediaCasterList");
        m_list->setDragDropMode(QAbstractItemView::InternalMove);
        lay->addWidget(m_list, 1);

        auto* btnRow = new QHBoxLayout;
        auto* addBtn = new QPushButton("+ Add Media");
        auto* removeBtn = new QPushButton("Remove");
        auto* clearBtn = new QPushButton("Clear All");
        addBtn->setObjectName("MediaCasterAddBtn");
        removeBtn->setObjectName("MediaCasterRemoveBtn");
        clearBtn->setObjectName("MediaCasterClearBtn");
        btnRow->addWidget(addBtn);
        btnRow->addWidget(removeBtn);
        btnRow->addWidget(clearBtn);
        lay->addLayout(btnRow);

        connect(addBtn, &QPushButton::clicked, this, &PlaylistPanel::onAddMedia);
        connect(removeBtn, &QPushButton::clicked, this, &PlaylistPanel::onRemoveMedia);
        connect(clearBtn, &QPushButton::clicked, this, &PlaylistPanel::onClearAll);
        connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            int row = m_list->row(item);
            emit loadRequested(row);
        });
    }

    void rebuild() {
        m_list->clear();
        for (int i = 0; i < m_mod->mediaItemCount(); ++i) {
            auto item = m_mod->mediaItem(i);
            QString label = item.title.isEmpty()
                ? QFileInfo(item.filePath.toLocalFile()).fileName()
                : item.title;
            if (item.isVideo)
                label = QString::fromUtf8("\xF0\x9F\x8E\xAC ") + label;  // film clapper
            else
                label = QString::fromUtf8("\xF0\x9F\x8E\xB5 ") + label;  // music note
            if (item.looping)
                label += " [Loop]";
            m_list->addItem(label);
        }
    }

    void setCurrentIndex(int idx) {
        if (idx >= 0 && idx < m_list->count())
            m_list->setCurrentRow(idx);
    }

signals:
    void loadRequested(int index);

private slots:
    void onAddMedia() {
        QStringList files = QFileDialog::getOpenFileNames(
            this, "Add Media Files", QString(),
            "Media Files (*.mp4 *.mov *.avi *.webm *.mkv *.mp3 *.wav *.ogg *.flac *.aac);;"
            "Video Files (*.mp4 *.mov *.avi *.webm *.mkv);;"
            "Audio Files (*.mp3 *.wav *.ogg *.flac *.aac);;"
            "All Files (*)");
        for (const auto& file : files) {
            QFileInfo fi(file);
            M1::ServiceMediaItem item;
            item.title    = fi.completeBaseName();
            item.filePath = QUrl::fromLocalFile(file);
            // We detect video vs audio by extension
            static const QStringList audioExts = {"mp3", "wav", "ogg", "flac", "aac", "wma"};
            item.isVideo = !audioExts.contains(fi.suffix().toLower());
            m_mod->addMediaItem(item);
        }
        rebuild();
    }

    void onRemoveMedia() {
        int row = m_list->currentRow();
        if (row >= 0) {
            m_mod->removeMediaItem(row);
            rebuild();
        }
    }

    void onClearAll() {
        m_mod->clearPlaylist();
        rebuild();
    }

private:
    M1::MediaCasterModule* m_mod;
    QListWidget* m_list = nullptr;
};

// ─── TransportPanel — play/pause/stop, seek, volume, cue points ─────────────
class TransportPanel : public QWidget {
    Q_OBJECT
public:
    explicit TransportPanel(M1::MediaCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("MediaCasterTransport");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);

        // ── Now Playing info ────────────────────────────────────────────────
        m_nowPlaying = new QLabel("No media loaded");
        m_nowPlaying->setObjectName("MediaCasterNowPlaying");
        lay->addWidget(m_nowPlaying);

        // ── Position slider + time labels ───────────────────────────────────
        auto* posRow = new QHBoxLayout;
        m_posLabel = new QLabel("0:00");
        m_posLabel->setObjectName("MediaCasterPosLabel");
        m_seekSlider = new QSlider(Qt::Horizontal);
        m_seekSlider->setObjectName("MediaCasterSeekSlider");
        m_seekSlider->setRange(0, 1000);
        m_durLabel = new QLabel("0:00");
        m_durLabel->setObjectName("MediaCasterDurLabel");
        posRow->addWidget(m_posLabel);
        posRow->addWidget(m_seekSlider, 1);
        posRow->addWidget(m_durLabel);
        lay->addLayout(posRow);

        // ── Transport buttons ───────────────────────────────────────────────
        auto* btnRow = new QHBoxLayout;
        m_playBtn  = new QPushButton("Play");
        m_pauseBtn = new QPushButton("Pause");
        m_stopBtn  = new QPushButton("Stop");
        m_playBtn->setObjectName("MediaCasterPlayBtn");
        m_pauseBtn->setObjectName("MediaCasterPauseBtn");
        m_stopBtn->setObjectName("MediaCasterStopBtn");
        m_pauseBtn->setEnabled(false);
        btnRow->addWidget(m_playBtn);
        btnRow->addWidget(m_pauseBtn);
        btnRow->addWidget(m_stopBtn);
        lay->addLayout(btnRow);

        // ── Volume + Loop + Preview controls ────────────────────────────────
        auto* ctrlRow = new QHBoxLayout;
        ctrlRow->addWidget(new QLabel("Vol:"));
        m_volSlider = new QSlider(Qt::Horizontal);
        m_volSlider->setObjectName("MediaCasterVolSlider");
        m_volSlider->setRange(0, 100);
        m_volSlider->setValue(100);
        ctrlRow->addWidget(m_volSlider, 1);

        m_loopCheck = new QCheckBox("Loop");
        m_loopCheck->setObjectName("MediaCasterLoopCheck");
        ctrlRow->addWidget(m_loopCheck);

        m_previewCheck = new QCheckBox("Preview");
        m_previewCheck->setObjectName("MediaCasterPreviewCheck");
        m_previewCheck->setToolTip("Preview mode — does not send to live output");
        ctrlRow->addWidget(m_previewCheck);
        lay->addLayout(ctrlRow);

        // ── Cue points ─────────────────────────────────────────────────────
        auto* cueGroup = new QGroupBox("Cue Points");
        cueGroup->setObjectName("MediaCasterCueGroup");
        auto* cueLay = new QVBoxLayout(cueGroup);
        m_cueList = new QListWidget;
        m_cueList->setObjectName("MediaCasterCueList");
        m_cueList->setMaximumHeight(100);
        cueLay->addWidget(m_cueList);

        auto* cueRow = new QHBoxLayout;
        auto* addCueBtn = new QPushButton("+ Cue Here");
        addCueBtn->setObjectName("MediaCasterAddCueBtn");
        auto* jumpCueBtn = new QPushButton("Jump to Cue");
        jumpCueBtn->setObjectName("MediaCasterJumpCueBtn");
        cueRow->addWidget(addCueBtn);
        cueRow->addWidget(jumpCueBtn);
        cueLay->addLayout(cueRow);
        lay->addWidget(cueGroup);

        // ── Connections ─────────────────────────────────────────────────────
        connect(m_playBtn, &QPushButton::clicked, this, [this]() {
            m_mod->play();
        });
        connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
            m_mod->pause();
        });
        connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
            m_mod->stop();
        });
        connect(m_volSlider, &QSlider::valueChanged, this, [this](int val) {
            m_mod->setVolume(val / 100.0);
        });
        connect(m_loopCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_mod->setLooping(checked);
        });
        connect(m_previewCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_mod->setPreviewMode(checked);
        });
        connect(m_seekSlider, &QSlider::sliderMoved, this, [this](int val) {
            qint64 pos = (m_mod->duration() * val) / 1000;
            m_mod->seekTo(pos);
        });
        connect(addCueBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->currentIndex() < 0) return;
            bool ok = false;
            QString label = QInputDialog::getText(this, "Add Cue Point",
                "Cue label:", QLineEdit::Normal, "", &ok);
            if (!ok || label.isEmpty()) return;

            auto item = m_mod->mediaItem(m_mod->currentIndex());
            M1::ServiceCuePoint cue;
            cue.label      = label;
            cue.positionMs = m_mod->position();
            item.cuePoints.append(cue);
            // We re-add the item with the new cue point (remove + insert)
            int idx = m_mod->currentIndex();
            m_mod->removeMediaItem(idx);
            // Re-insert at same position by adding at end then... we just update
            // For simplicity we rebuild the whole item
            m_mod->addMediaItem(item);
            refreshCueList();
        });
        connect(jumpCueBtn, &QPushButton::clicked, this, [this]() {
            int row = m_cueList->currentRow();
            if (row >= 0) m_mod->jumpToCue(row);
        });

        // ── Module signal connections ───────────────────────────────────────
        connect(m_mod, &M1::MediaCasterModule::playbackStateChanged, this, [this](bool playing) {
            m_playBtn->setEnabled(!playing);
            m_pauseBtn->setEnabled(playing);
        });
        connect(m_mod, &M1::MediaCasterModule::positionChanged, this, &TransportPanel::onPositionUpdate);
        connect(m_mod, &M1::MediaCasterModule::durationChanged, this, [this](qint64 dur) {
            m_durLabel->setText(formatTime(dur));
        });
        connect(m_mod, &M1::MediaCasterModule::mediaLoaded, this, [this](int idx) {
            if (idx >= 0 && idx < m_mod->mediaItemCount()) {
                auto item = m_mod->mediaItem(idx);
                m_nowPlaying->setText(item.title.isEmpty()
                    ? QFileInfo(item.filePath.toLocalFile()).fileName()
                    : item.title);
                refreshCueList();
            }
        });
    }

    void onPositionUpdate(qint64 posMs) {
        m_posLabel->setText(formatTime(posMs));
        if (!m_seekSlider->isSliderDown() && m_mod->duration() > 0) {
            int sliderVal = static_cast<int>((posMs * 1000) / m_mod->duration());
            m_seekSlider->setValue(sliderVal);
        }
    }

    void refreshCueList() {
        m_cueList->clear();
        if (m_mod->currentIndex() < 0) return;
        auto item = m_mod->mediaItem(m_mod->currentIndex());
        for (const auto& cue : item.cuePoints) {
            m_cueList->addItem(QString("[%1] %2").arg(formatTime(cue.positionMs), cue.label));
        }
    }

private:
    M1::MediaCasterModule* m_mod;
    QLabel*       m_nowPlaying = nullptr;
    QSlider*      m_seekSlider = nullptr;
    QLabel*       m_posLabel   = nullptr;
    QLabel*       m_durLabel   = nullptr;
    QPushButton*  m_playBtn    = nullptr;
    QPushButton*  m_pauseBtn   = nullptr;
    QPushButton*  m_stopBtn    = nullptr;
    QSlider*      m_volSlider  = nullptr;
    QCheckBox*    m_loopCheck  = nullptr;
    QCheckBox*    m_previewCheck = nullptr;
    QListWidget*  m_cueList    = nullptr;
};

// ─── MediaCasterWidget — main composite widget ─────────────────────────────
class MediaCasterWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaCasterWidget(M1::MediaCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("MediaCasterWidget");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        auto* splitter = new QSplitter(Qt::Horizontal);

        // Left: playlist
        m_playlistPanel = new PlaylistPanel(mod);
        splitter->addWidget(m_playlistPanel);

        // Right: preview + transport
        auto* rightPanel = new QWidget;
        auto* rightLay = new QVBoxLayout(rightPanel);
        rightLay->setContentsMargins(0, 0, 0, 0);

        m_preview = new VideoPreview;
        rightLay->addWidget(m_preview, 1);

        m_transport = new TransportPanel(mod);
        rightLay->addWidget(m_transport);

        splitter->addWidget(rightPanel);
        splitter->setStretchFactor(0, 1);  // playlist gets 1 part
        splitter->setStretchFactor(1, 3);  // preview+transport gets 3 parts

        lay->addWidget(splitter);

        // Wire playlist double-click to load
        connect(m_playlistPanel, &PlaylistPanel::loadRequested, this, [this](int idx) {
            m_mod->loadItem(idx);
            m_playlistPanel->setCurrentIndex(idx);
        });

        // Wire playlist changes to rebuild
        connect(m_mod, &M1::MediaCasterModule::playlistChanged, this, [this]() {
            m_playlistPanel->rebuild();
        });
    }

    QVideoWidget* videoWidget() { return m_preview->videoWidget(); }
    VideoPreview* preview()     { return m_preview; }

private:
    M1::MediaCasterModule* m_mod;
    PlaylistPanel*  m_playlistPanel = nullptr;
    VideoPreview*   m_preview       = nullptr;
    TransportPanel* m_transport     = nullptr;
};

} // anonymous namespace

// ─── MOC include for anonymous namespace QObjects ───────────────────────────
#include "MediaCasterModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
MediaCasterModule::MediaCasterModule(QObject* parent)
    : IModule(parent)
{
}

MediaCasterModule::~MediaCasterModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void MediaCasterModule::initialize() {
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(250);  // 4Hz position update
    connect(m_positionTimer, &QTimer::timeout, this, &MediaCasterModule::onPositionTick);
}

void MediaCasterModule::shutdown() {
    stop();
    if (m_positionTimer) m_positionTimer->stop();
}

QWidget* MediaCasterModule::createWidget(QWidget* parent) {
    initialize();

    auto* widget = new MediaCasterWidget(this, parent);

    // We create a QMediaPlayer bound to the video widget surface
    auto* player = new QMediaPlayer(widget);
    auto* audioOut = new QAudioOutput(widget);
    player->setAudioOutput(audioOut);
    player->setVideoOutput(widget->videoWidget());

    // We store the player pointer using dynamic property for access
    widget->setProperty("_m1_player", QVariant::fromValue(static_cast<QObject*>(player)));
    widget->setProperty("_m1_audioOut", QVariant::fromValue(static_cast<QObject*>(audioOut)));

    // Connect player signals to our module state
    connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        m_positionMs = pos;
        emit positionChanged(pos);
    });
    connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        m_durationMs = dur;
        emit durationChanged(dur);
    });
    connect(player, &QMediaPlayer::playbackStateChanged, this,
        [this, player, widget](QMediaPlayer::PlaybackState state) {
            m_playing = (state == QMediaPlayer::PlayingState);
            m_paused  = (state == QMediaPlayer::PausedState);
            emit playbackStateChanged(m_playing);
            if (state == QMediaPlayer::StoppedState && m_playing) {
                // We just finished — check for loop
                if (m_looping) {
                    player->setPosition(0);
                    player->play();
                } else {
                    emit mediaFinished();
                }
            }
            // Show/hide video widget
            if (state == QMediaPlayer::StoppedState)
                widget->preview()->showPlaceholder();
            else
                widget->preview()->showVideo();
        });
    connect(player, &QMediaPlayer::mediaStatusChanged, this,
        [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                m_playing = false;
                m_paused  = false;
                emit playbackStateChanged(false);
                if (m_looping) {
                    // Handled above in playbackStateChanged
                } else {
                    emit mediaFinished();
                }
            }
        });

    m_positionTimer->start();
    return widget;
}

void MediaCasterModule::saveState(QSettings& s) {
    s.beginGroup("MediaCaster");
    s.setValue("volume", m_volume);
    s.setValue("looping", m_looping);
    s.setValue("previewMode", m_previewMode);
    s.setValue("playlistCount", m_playlist.size());
    for (int i = 0; i < m_playlist.size(); ++i) {
        s.beginGroup(QString("item_%1").arg(i));
        s.setValue("title", m_playlist[i].title);
        s.setValue("filePath", m_playlist[i].filePath.toString());
        s.setValue("isVideo", m_playlist[i].isVideo);
        s.setValue("looping", m_playlist[i].looping);
        s.setValue("cueCount", m_playlist[i].cuePoints.size());
        for (int c = 0; c < m_playlist[i].cuePoints.size(); ++c) {
            s.setValue(QString("cue_%1_label").arg(c), m_playlist[i].cuePoints[c].label);
            s.setValue(QString("cue_%1_pos").arg(c), m_playlist[i].cuePoints[c].positionMs);
        }
        s.endGroup();
    }
    s.endGroup();
}

void MediaCasterModule::loadState(QSettings& s) {
    s.beginGroup("MediaCaster");
    m_volume = s.value("volume", 1.0).toDouble();
    m_looping = s.value("looping", false).toBool();
    m_previewMode = s.value("previewMode", false).toBool();
    int count = s.value("playlistCount", 0).toInt();
    m_playlist.clear();
    for (int i = 0; i < count; ++i) {
        s.beginGroup(QString("item_%1").arg(i));
        ServiceMediaItem item;
        item.title    = s.value("title").toString();
        item.filePath = QUrl(s.value("filePath").toString());
        item.isVideo  = s.value("isVideo", true).toBool();
        item.looping  = s.value("looping", false).toBool();
        int cueCount  = s.value("cueCount", 0).toInt();
        for (int c = 0; c < cueCount; ++c) {
            ServiceCuePoint cue;
            cue.label      = s.value(QString("cue_%1_label").arg(c)).toString();
            cue.positionMs = s.value(QString("cue_%1_pos").arg(c), 0).toLongLong();
            item.cuePoints.append(cue);
        }
        m_playlist.append(item);
        s.endGroup();
    }
    s.endGroup();
    emit playlistChanged();
}

// ─── Playlist management ────────────────────────────────────────────────────
void MediaCasterModule::addMediaItem(const ServiceMediaItem& item) {
    m_playlist.append(item);
    emit playlistChanged();
}

void MediaCasterModule::removeMediaItem(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_playlist.removeAt(index);
    if (m_currentIndex >= m_playlist.size())
        m_currentIndex = m_playlist.size() - 1;
    emit playlistChanged();
}

void MediaCasterModule::clearPlaylist() {
    stop();
    m_playlist.clear();
    m_currentIndex = -1;
    emit playlistChanged();
}

ServiceMediaItem MediaCasterModule::mediaItem(int index) const {
    if (index < 0 || index >= m_playlist.size()) return {};
    return m_playlist[index];
}

// ─── Playback control ───────────────────────────────────────────────────────
void MediaCasterModule::loadItem(int index) {
    if (index < 0 || index >= m_playlist.size()) return;
    m_currentIndex = index;

    // We find the QMediaPlayer associated with our widget
    auto* w = qobject_cast<QWidget*>(children().isEmpty() ? nullptr : children().constFirst());
    // Walk the widget tree to find the player stored as property
    QList<QWidget*> widgets;
    // We iterate all children of the module to find the MediaCasterWidget
    for (auto* child : children()) {
        auto* cw = qobject_cast<QWidget*>(child);
        if (cw) widgets.append(cw);
    }
    // Actually, we store the player on the MediaCasterWidget via property
    // We need to find it through the widget hierarchy
    // The createWidget() stores it on the widget, so we can find it
    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* player = qobject_cast<QMediaPlayer*>(
                child->property("_m1_player").value<QObject*>());
            if (player) {
                player->setSource(m_playlist[index].filePath);
                auto* audioOut = qobject_cast<QAudioOutput*>(
                    child->property("_m1_audioOut").value<QObject*>());
                if (audioOut)
                    audioOut->setVolume(static_cast<float>(m_volume));
            }
            break;
        }
    }

    m_positionMs = 0;
    m_durationMs = 0;
    emit mediaLoaded(index);
}

void MediaCasterModule::play() {
    if (m_currentIndex < 0 && !m_playlist.isEmpty())
        loadItem(0);

    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* player = qobject_cast<QMediaPlayer*>(
                child->property("_m1_player").value<QObject*>());
            if (player) {
                player->play();
                m_playing = true;
                m_paused  = false;
                emit playbackStateChanged(true);
            }
            break;
        }
    }
}

void MediaCasterModule::pause() {
    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* player = qobject_cast<QMediaPlayer*>(
                child->property("_m1_player").value<QObject*>());
            if (player) {
                player->pause();
                m_paused  = true;
                m_playing = false;
                emit playbackStateChanged(false);
            }
            break;
        }
    }
}

void MediaCasterModule::stop() {
    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* player = qobject_cast<QMediaPlayer*>(
                child->property("_m1_player").value<QObject*>());
            if (player) {
                player->stop();
                m_playing = false;
                m_paused  = false;
                m_positionMs = 0;
                emit playbackStateChanged(false);
                emit positionChanged(0);
            }
            break;
        }
    }
}

void MediaCasterModule::seekTo(qint64 positionMs) {
    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* player = qobject_cast<QMediaPlayer*>(
                child->property("_m1_player").value<QObject*>());
            if (player) {
                player->setPosition(positionMs);
                m_positionMs = positionMs;
                emit positionChanged(positionMs);
            }
            break;
        }
    }
}

void MediaCasterModule::jumpToCue(int cueIndex) {
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) return;
    const auto& cues = m_playlist[m_currentIndex].cuePoints;
    if (cueIndex < 0 || cueIndex >= cues.size()) return;
    seekTo(cues[cueIndex].positionMs);
    emit cuePointReached(cueIndex, cues[cueIndex].label);
}

void MediaCasterModule::setVolume(double volume) {
    m_volume = qBound(0.0, volume, 1.0);
    for (auto* child : QApplication::allWidgets()) {
        if (child->objectName() == "MediaCasterWidget") {
            auto* audioOut = qobject_cast<QAudioOutput*>(
                child->property("_m1_audioOut").value<QObject*>());
            if (audioOut)
                audioOut->setVolume(static_cast<float>(m_volume));
            break;
        }
    }
}

void MediaCasterModule::setLooping(bool loop) {
    m_looping = loop;
}

void MediaCasterModule::setPreviewMode(bool preview) {
    m_previewMode = preview;
}

void MediaCasterModule::onPositionTick() {
    // Position updates are driven by QMediaPlayer::positionChanged signal
    // We use this timer for any periodic housekeeping
    if (!m_playing) return;

    // Check cue points — fire signal when we cross a cue
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        const auto& cues = m_playlist[m_currentIndex].cuePoints;
        for (int i = 0; i < cues.size(); ++i) {
            qint64 cuePos = cues[i].positionMs;
            // We fire if we're within 300ms of the cue point (accounting for timer granularity)
            if (m_positionMs >= cuePos && m_positionMs < cuePos + 300) {
                emit cuePointReached(i, cues[i].label);
            }
        }
    }
}

} // namespace M1
