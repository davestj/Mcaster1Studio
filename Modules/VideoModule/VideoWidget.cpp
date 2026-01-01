#include "VideoWidget.h"
#include "VideoModule.h"
#include <QFileDialog>
#include <QListWidgetItem>
#include <QFileInfo>
#include <QSizePolicy>
#include <QFrame>
#include <QScrollBar>
#include <QToolTip>

// ─── VideoWidget ──────────────────────────────────────────────────────────────

VideoWidget::VideoWidget(M1::VideoModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    buildUi();
    applyStyles();

    // Wire module signals → widget slots
    connect(m_module, &M1::VideoModule::nowPlayingChanged,
            this, &VideoWidget::onNowPlayingChanged);
    connect(m_module, &M1::VideoModule::playlistChanged,
            this, &VideoWidget::onPlaylistChanged);
    connect(m_module, &M1::VideoModule::stateChanged,
            this, &VideoWidget::onModuleStateChanged);

    // Wire QMediaPlayer signals for position / duration overlay
    QMediaPlayer* player = m_module->mediaPlayer();
    connect(player, &QMediaPlayer::positionChanged,
            this, &VideoWidget::onPositionChanged);
    connect(player, &QMediaPlayer::durationChanged,
            this, &VideoWidget::onDurationChanged);

    // Attach QVideoWidget as the video output surface
    player->setVideoOutput(m_videoWidget);

    // Populate playlist view from any pre-loaded state
    onPlaylistChanged();
}

// ─── UI construction ──────────────────────────────────────────────────────────

void VideoWidget::buildUi() {
    setObjectName("VideoWidget");

    // ── Video area with overlay ───────────────────────────────────────────
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setObjectName("VideoPreview");
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoWidget->setMinimumSize(320, 180);

    // Info overlay (bottom of video area, anchored via absolute positioning)
    m_infoOverlay = new QLabel(m_videoWidget);
    m_infoOverlay->setObjectName("VideoInfoOverlay");
    m_infoOverlay->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_infoOverlay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_infoOverlay->setMinimumHeight(20);
    m_infoOverlay->setMargin(4);
    m_infoOverlay->setText("No file loaded");

    // We position the overlay at the bottom of the video widget via a layout
    // overlaid on top of it. A thin widget with a QVBoxLayout stretches first,
    // then the label at the bottom.
    auto* videoContainer = new QWidget(this);
    videoContainer->setObjectName("VideoContainer");
    videoContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* overlayLayout = new QVBoxLayout(videoContainer);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(m_videoWidget);

    // Overlay label lives inside its own absolute layout within videoContainer
    // We recreate the video widget container properly using stacked approach:
    // The QVideoWidget takes the full container, the label is overlaid at bottom.
    // Remove previous layout and rebuild:
    delete videoContainer->layout();

    auto* vcLayout = new QVBoxLayout(videoContainer);
    vcLayout->setContentsMargins(0, 0, 0, 0);
    vcLayout->setSpacing(0);

    // Stack: full-size video widget
    m_videoWidget->setParent(videoContainer);
    vcLayout->addWidget(m_videoWidget, 1);

    // Info overlay: separate from the video widget to avoid OpenGL surface issues
    m_infoOverlay->setParent(videoContainer);

    // We'll use an event filter / resize override style:
    // Place the overlay as a child of the container, positioned at bottom.
    // This is done in resizeEvent via installEventFilter, but for simplicity
    // we use a second pass in the outer layout using a QFrame at the bottom:
    auto* infoBar = new QFrame(videoContainer);
    infoBar->setObjectName("VideoInfoBar");
    infoBar->setMinimumHeight(24);
    infoBar->setFrameShape(QFrame::NoFrame);
    auto* infoBarLayout = new QHBoxLayout(infoBar);
    infoBarLayout->setContentsMargins(6, 2, 6, 2);

    // Repurpose m_infoOverlay as child of infoBar
    m_infoOverlay->setParent(infoBar);
    m_infoOverlay->setMinimumHeight(20);
    infoBarLayout->addWidget(m_infoOverlay, 1);

    vcLayout->addWidget(infoBar, 0);

    // ── Playlist panel ────────────────────────────────────────────────────
    m_playlistPanel = new QWidget(this);
    m_playlistPanel->setObjectName("PlaylistPanel");
    m_playlistPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_playlistPanel->setMinimumWidth(180);
    m_playlistPanel->setMaximumWidth(600);

    m_playlistView = new QListWidget(m_playlistPanel);
    m_playlistView->setObjectName("PlaylistView");
    m_playlistView->setDragDropMode(QAbstractItemView::InternalMove);
    m_playlistView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_addFilesBtn = new QPushButton("+ Add Files", m_playlistPanel);
    m_addFilesBtn->setObjectName("AddFilesBtn");

    auto* plLayout = new QVBoxLayout(m_playlistPanel);
    plLayout->setContentsMargins(4, 4, 4, 4);
    plLayout->setSpacing(4);
    plLayout->addWidget(m_playlistView, 1);
    plLayout->addWidget(m_addFilesBtn, 0);

    // ── Splitter: video area | playlist panel ─────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("VideoSplitter");
    m_splitter->addWidget(videoContainer);
    m_splitter->addWidget(m_playlistPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    // ── Transport controls row ────────────────────────────────────────────
    auto* controlsRow = new QWidget(this);
    controlsRow->setObjectName("VideoControls");
    controlsRow->setMinimumHeight(36);

    m_prevBtn       = new QPushButton("|<", controlsRow);
    m_playPauseBtn  = new QPushButton("Play", controlsRow);
    m_stopBtn       = new QPushButton("Stop", controlsRow);
    m_nextBtn       = new QPushButton(">|", controlsRow);

    m_prevBtn->setObjectName("TransportBtn");
    m_playPauseBtn->setObjectName("TransportBtnAccent");
    m_stopBtn->setObjectName("TransportBtn");
    m_nextBtn->setObjectName("TransportBtn");

    m_prevBtn->setMinimumWidth(36);
    m_playPauseBtn->setMinimumWidth(56);
    m_stopBtn->setMinimumWidth(44);
    m_nextBtn->setMinimumWidth(36);

    // Toggle playlist visibility
    m_toggleListBtn = new QPushButton("List", controlsRow);
    m_toggleListBtn->setObjectName("TransportBtn");
    m_toggleListBtn->setCheckable(true);
    m_toggleListBtn->setChecked(true);
    m_toggleListBtn->setMinimumWidth(36);

    // Volume slider
    auto* volLabel = new QLabel("Vol:", controlsRow);
    volLabel->setObjectName("ControlLabel");
    m_volumeSlider = new QSlider(Qt::Horizontal, controlsRow);
    m_volumeSlider->setObjectName("VolumeSlider");
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setMinimumWidth(70);

    // RTMP stub checkbox
    // TODO: Phase 9b — wire this checkbox to the FFmpeg RTMP push pipeline
    m_rtmpCheckBox = new QCheckBox("RTMP Stream", controlsRow);
    m_rtmpCheckBox->setObjectName("RTMPCheckBox");
    m_rtmpCheckBox->setEnabled(false); // disabled until Phase 9b
    m_rtmpCheckBox->setToolTip(
        "RTMP streaming requires Phase 9b (FFmpeg integration)"
    );

    auto* ctrlLayout = new QHBoxLayout(controlsRow);
    ctrlLayout->setContentsMargins(8, 4, 8, 4);
    ctrlLayout->setSpacing(6);
    ctrlLayout->addWidget(m_prevBtn);
    ctrlLayout->addWidget(m_playPauseBtn);
    ctrlLayout->addWidget(m_stopBtn);
    ctrlLayout->addWidget(m_nextBtn);
    ctrlLayout->addSpacing(12);
    ctrlLayout->addWidget(m_toggleListBtn);
    ctrlLayout->addStretch(1);
    ctrlLayout->addWidget(volLabel);
    ctrlLayout->addWidget(m_volumeSlider);
    ctrlLayout->addSpacing(12);
    ctrlLayout->addWidget(m_rtmpCheckBox);

    // ── Main layout ───────────────────────────────────────────────────────
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_splitter, 1);
    mainLayout->addWidget(controlsRow, 0);

    // ── Signal connections ────────────────────────────────────────────────
    connect(m_prevBtn,      &QPushButton::clicked, this, &VideoWidget::onPreviousClicked);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoWidget::onPlayPauseClicked);
    connect(m_stopBtn,      &QPushButton::clicked, this, &VideoWidget::onStopClicked);
    connect(m_nextBtn,      &QPushButton::clicked, this, &VideoWidget::onNextClicked);
    connect(m_addFilesBtn,  &QPushButton::clicked, this, &VideoWidget::onAddFilesClicked);
    connect(m_toggleListBtn,&QPushButton::clicked, this, &VideoWidget::onTogglePlaylist);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &VideoWidget::onVolumeChanged);

    connect(m_playlistView, &QListWidget::itemDoubleClicked,
            this, &VideoWidget::onPlaylistDoubleClicked);
}

void VideoWidget::applyStyles() {
    // Styles are provided by the application theme QSS (objectName selectors).
    // Clearing any previously set inline stylesheet ensures theme QSS wins.
    setStyleSheet(QString());
}

// ─── Slot implementations ─────────────────────────────────────────────────────

void VideoWidget::onPlayPauseClicked() {
    using State = M1::VideoModule::State;
    if (m_module->playbackState() == State::Playing) {
        m_module->pause();
    } else if (m_module->playbackState() == State::Paused) {
        m_module->play();
    } else {
        m_module->play();
    }
}

void VideoWidget::onStopClicked() {
    m_module->stop();
}

void VideoWidget::onNextClicked() {
    m_module->next();
}

void VideoWidget::onPreviousClicked() {
    m_module->previous();
}

void VideoWidget::onAddFilesClicked() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "Add Video Files",
        QString(),
        "Video Files (*.mp4 *.mkv *.avi *.mov *.wmv *.webm);;"
        "All Files (*.*)"
    );
    if (!files.isEmpty()) {
        m_module->addToPlaylist(files);
    }
}

void VideoWidget::onPlaylistDoubleClicked(QListWidgetItem* item) {
    int row = m_playlistView->row(item);
    m_module->playAtIndex(row);
}

void VideoWidget::onNowPlayingChanged(const QString& path) {
    QFileInfo fi(path);
    updateInfoOverlay(0, m_durationMs);
    m_playlistView->setCurrentRow(m_module->currentIndex());
    // Update Play/Pause button to show "Pause"
    m_playPauseBtn->setText("Pause");
}

void VideoWidget::onPlaylistChanged() {
    m_playlistView->clear();
    const QList<QString>& pl = m_module->playlist();
    for (const QString& p : pl) {
        QFileInfo fi(p);
        auto* item = new QListWidgetItem(fi.fileName(), m_playlistView);
        item->setToolTip(p);
    }
    // Highlight current item
    int cur = m_module->currentIndex();
    if (cur >= 0 && cur < m_playlistView->count())
        m_playlistView->setCurrentRow(cur);
}

void VideoWidget::onModuleStateChanged(M1::VideoModule::State state) {
    using State = M1::VideoModule::State;
    switch (state) {
    case State::Playing:
        m_playPauseBtn->setText("Pause");
        break;
    case State::Paused:
        m_playPauseBtn->setText("Play");
        break;
    case State::Idle:
        m_playPauseBtn->setText("Play");
        break;
    }
}

void VideoWidget::onPositionChanged(qint64 pos) {
    updateInfoOverlay(pos, m_durationMs);
}

void VideoWidget::onDurationChanged(qint64 dur) {
    m_durationMs = dur;
    updateInfoOverlay(m_module->mediaPlayer()->position(), dur);
}

void VideoWidget::onVolumeChanged(int value) {
    m_module->audioOutput()->setVolume(value / 100.0f);
}

void VideoWidget::onTogglePlaylist() {
    m_playlistPanel->setVisible(m_toggleListBtn->isChecked());
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void VideoWidget::updateInfoOverlay(qint64 posMs, qint64 durMs) {
    int idx = m_module->currentIndex();
    const QList<QString>& pl = m_module->playlist();
    QString filename;
    if (idx >= 0 && idx < pl.size()) {
        filename = QFileInfo(pl[idx]).fileName();
    } else {
        QUrl src = m_module->mediaPlayer()->source();
        if (!src.isEmpty())
            filename = src.fileName();
    }

    if (filename.isEmpty()) {
        m_infoOverlay->setText("No file loaded");
        return;
    }

    const QString pos = formatTime(posMs);
    const QString dur = formatTime(durMs);
    m_infoOverlay->setText(QString("%1   %2 / %3").arg(filename, pos, dur));
}

QString VideoWidget::formatTime(qint64 ms) {
    if (ms < 0) ms = 0;
    const qint64 secs = ms / 1000;
    const qint64 mins = secs / 60;
    const qint64 hrs  = mins / 60;
    if (hrs > 0)
        return QString("%1:%2:%3")
            .arg(hrs)
            .arg(mins % 60, 2, 10, QChar('0'))
            .arg(secs % 60, 2, 10, QChar('0'));
    return QString("%1:%2")
        .arg(mins % 60, 2, 10, QChar('0'))
        .arg(secs % 60, 2, 10, QChar('0'));
}
