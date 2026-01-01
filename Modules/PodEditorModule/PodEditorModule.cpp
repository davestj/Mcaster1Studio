/// @file   PodEditorModule.cpp
/// @path   Modules/PodEditorModule/PodEditorModule.cpp

#include "PodEditorModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollBar>
#include <QToolBar>
#include <QFileDialog>
#include <QSettings>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>

namespace {

// ─── TimelineRuler — time ruler above waveform ─────────────────────────────
class TimelineRuler : public QWidget {
public:
    explicit TimelineRuler(M1::PodEditorModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodEditorRuler");
        setFixedHeight(24);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));

        if (m_mod->duration() <= 0) return;

        const qint64 durationMs = m_mod->duration();
        const float zoom = m_mod->zoomLevel();
        const float scroll = m_mod->scrollPosition();
        const double totalWidth = width() * zoom;
        const double offsetX = scroll * (totalWidth - width());

        // Calculate tick interval based on zoom
        double tickIntervalMs = 1000.0; // 1 second
        if (zoom > 4.0f) tickIntervalMs = 500.0;
        if (zoom > 8.0f) tickIntervalMs = 100.0;
        if (zoom < 0.5f) tickIntervalMs = 5000.0;

        p.setPen(QColor(0x94, 0xa3, 0xb8));
        QFont f = font();
        f.setPixelSize(9);
        p.setFont(f);

        for (double ms = 0; ms <= durationMs; ms += tickIntervalMs) {
            const double x = (ms / durationMs) * totalWidth - offsetX;
            if (x < -10 || x > width() + 10) continue;

            const int ix = static_cast<int>(x);
            const bool major = (static_cast<int>(ms) % 5000 == 0);

            p.drawLine(ix, major ? 4 : 12, ix, height());

            if (major) {
                const int totalSec = static_cast<int>(ms / 1000);
                const int min = totalSec / 60;
                const int sec = totalSec % 60;
                p.drawText(ix + 3, 14, QString("%1:%2")
                    .arg(min).arg(sec, 2, 10, QChar('0')));
            }
        }
    }

private:
    M1::PodEditorModule* m_mod;
};

// ─── WaveformView — main waveform display ───────────────────────────────────
class WaveformView : public QWidget {
    Q_OBJECT
public:
    explicit WaveformView(M1::PodEditorModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodEditorWaveform");
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.fillRect(rect(), QColor(0x0f, 0x17, 0x2a));

        const auto& wf = m_mod->waveformData();
        if (wf.peaks.isEmpty() || wf.durationMs <= 0) {
            // Empty state
            p.setPen(QColor(0x64, 0x74, 0x8b));
            QFont f = font();
            f.setPixelSize(14);
            p.setFont(f);
            p.drawText(rect(), Qt::AlignCenter, "No audio loaded\nDrag a file or click Open");
            return;
        }

        const float zoom = m_mod->zoomLevel();
        const float scroll = m_mod->scrollPosition();
        const double totalWidth = width() * zoom;
        const double offsetX = scroll * (totalWidth - width());
        const int centerY = height() / 2;

        // Draw selection region
        if (m_mod->hasSelection()) {
            const auto sel = m_mod->selection();
            const double selStartX = (static_cast<double>(sel.startMs) / wf.durationMs)
                                     * totalWidth - offsetX;
            const double selEndX   = (static_cast<double>(sel.endMs) / wf.durationMs)
                                     * totalWidth - offsetX;
            p.fillRect(QRectF(selStartX, 0, selEndX - selStartX, height()),
                       QColor(0x0e, 0xa5, 0xe9, 50));
            p.setPen(QPen(QColor(0x0e, 0xa5, 0xe9), 1));
            p.drawLine(QPointF(selStartX, 0), QPointF(selStartX, height()));
            p.drawLine(QPointF(selEndX, 0), QPointF(selEndX, height()));
        }

        // Draw waveform
        p.setPen(Qt::NoPen);
        const int peakCount = wf.peaks.size();

        for (int px = 0; px < width(); ++px) {
            // Map pixel to peak index
            const double worldX = px + offsetX;
            const double t = worldX / totalWidth;
            if (t < 0.0 || t > 1.0) continue;

            const int peakIdx = static_cast<int>(t * (peakCount - 1));
            if (peakIdx < 0 || peakIdx >= peakCount) continue;

            const float peak = wf.peaks[peakIdx];
            const int barH = static_cast<int>(peak * (height() / 2 - 4));

            // Gradient color based on amplitude
            QColor barColor;
            if (peak < 0.5f)
                barColor = QColor(0x22, 0xc5, 0x5e);
            else if (peak < 0.8f)
                barColor = QColor(0xf5, 0x9e, 0x0b);
            else
                barColor = QColor(0xef, 0x44, 0x44);

            p.setBrush(barColor);
            p.drawRect(px, centerY - barH, 1, barH * 2);
        }

        // Draw center line
        p.setPen(QPen(QColor(0x47, 0x55, 0x69), 1));
        p.drawLine(0, centerY, width(), centerY);

        // Draw markers
        p.setPen(QPen(QColor(0xf5, 0x9e, 0x0b), 2));
        const auto& markers = m_mod->markers();
        QFont mf = font();
        mf.setPixelSize(9);
        mf.setBold(true);
        p.setFont(mf);
        for (const auto& marker : markers) {
            const double mx = (static_cast<double>(marker.positionMs) / wf.durationMs)
                              * totalWidth - offsetX;
            if (mx < -10 || mx > width() + 10) continue;
            const int imx = static_cast<int>(mx);
            p.setPen(QPen(marker.color, 2));
            p.drawLine(imx, 0, imx, height());
            // Marker label
            p.setPen(marker.color);
            p.drawText(imx + 3, 12, marker.label);
        }

        // Draw playback position indicator
        if (m_mod->duration() > 0) {
            const qint64 pos = m_mod->position();
            const double posX = (static_cast<double>(pos) / wf.durationMs)
                                * totalWidth - offsetX;
            if (posX >= 0 && posX <= width()) {
                const int ipx = static_cast<int>(posX);
                p.setPen(QPen(QColor(0xff, 0xff, 0xff), 2));
                p.drawLine(ipx, 0, ipx, height());

                // Playhead triangle
                QPainterPath tri;
                tri.moveTo(ipx - 5, 0);
                tri.lineTo(ipx + 5, 0);
                tri.lineTo(ipx, 8);
                tri.closeSubpath();
                p.fillPath(tri, QColor(0xff, 0xff, 0xff));
            }
        }
    }

    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton && m_mod->duration() > 0) {
            m_dragging = true;
            const qint64 ms = pixelToMs(ev->pos().x());
            m_dragStartMs = ms;
            m_mod->seekTo(ms);
            m_mod->clearSelection();
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (m_dragging && m_mod->duration() > 0) {
            const qint64 ms = pixelToMs(ev->pos().x());
            const qint64 startMs = std::min(m_dragStartMs, ms);
            const qint64 endMs   = std::max(m_dragStartMs, ms);
            if (endMs - startMs > 50) { // minimum 50ms selection
                m_mod->setSelection(startMs, endMs);
            }
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            m_dragging = false;
        }
    }

    void wheelEvent(QWheelEvent* ev) override {
        if (ev->angleDelta().y() > 0)
            m_mod->zoomIn();
        else
            m_mod->zoomOut();
        update();
    }

private:
    qint64 pixelToMs(int px) const {
        const auto& wf = m_mod->waveformData();
        if (wf.durationMs <= 0) return 0;
        const float zoom = m_mod->zoomLevel();
        const float scroll = m_mod->scrollPosition();
        const double totalWidth = width() * zoom;
        const double offsetX = scroll * (totalWidth - width());
        const double t = (px + offsetX) / totalWidth;
        return static_cast<qint64>(std::clamp(t, 0.0, 1.0) * wf.durationMs);
    }

    M1::PodEditorModule* m_mod;
    bool   m_dragging    = false;
    qint64 m_dragStartMs = 0;
};

// ─── PodEditorWidget — main editor layout ───────────────────────────────────
class PodEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodEditorWidget(M1::PodEditorModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodEditorWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        // ── Toolbar ────────────────────────────────────────────────────────
        auto* toolbar = new QHBoxLayout;
        toolbar->setSpacing(4);

        auto* openBtn = new QPushButton("Open");
        openBtn->setObjectName("PodEditorOpenBtn");
        openBtn->setToolTip("Open audio file");
        connect(openBtn, &QPushButton::clicked, this, &PodEditorWidget::onOpen);
        toolbar->addWidget(openBtn);

        toolbar->addSpacing(8);

        // Transport
        m_playBtn = new QPushButton("Play");
        m_playBtn->setObjectName("PodEditorPlayBtn");
        m_playBtn->setToolTip("Play / Pause");
        connect(m_playBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->isPlaying()) m_mod->pause();
            else m_mod->play();
        });
        toolbar->addWidget(m_playBtn);

        auto* stopBtn = new QPushButton("Stop");
        stopBtn->setObjectName("PodEditorStopBtn");
        stopBtn->setToolTip("Stop playback");
        connect(stopBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::stop);
        toolbar->addWidget(stopBtn);

        toolbar->addSpacing(8);

        // Edit buttons
        auto* cutBtn = new QPushButton("Cut");
        cutBtn->setObjectName("PodEditorCutBtn");
        cutBtn->setToolTip("Cut selection");
        connect(cutBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::cut);
        toolbar->addWidget(cutBtn);

        auto* copyBtn = new QPushButton("Copy");
        copyBtn->setObjectName("PodEditorCopyBtn");
        copyBtn->setToolTip("Copy selection");
        connect(copyBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::copy);
        toolbar->addWidget(copyBtn);

        auto* pasteBtn = new QPushButton("Paste");
        pasteBtn->setObjectName("PodEditorPasteBtn");
        pasteBtn->setToolTip("Paste at cursor");
        connect(pasteBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::paste);
        toolbar->addWidget(pasteBtn);

        toolbar->addSpacing(8);

        // Zoom
        auto* zoomInBtn = new QPushButton("+");
        zoomInBtn->setFixedWidth(28);
        zoomInBtn->setObjectName("PodEditorZoomInBtn");
        zoomInBtn->setToolTip("Zoom in");
        connect(zoomInBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::zoomIn);
        toolbar->addWidget(zoomInBtn);

        auto* zoomOutBtn = new QPushButton("-");
        zoomOutBtn->setFixedWidth(28);
        zoomOutBtn->setObjectName("PodEditorZoomOutBtn");
        zoomOutBtn->setToolTip("Zoom out");
        connect(zoomOutBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::zoomOut);
        toolbar->addWidget(zoomOutBtn);

        auto* zoomFitBtn = new QPushButton("Fit");
        zoomFitBtn->setObjectName("PodEditorZoomFitBtn");
        zoomFitBtn->setToolTip("Zoom to fit");
        connect(zoomFitBtn, &QPushButton::clicked, mod, &M1::PodEditorModule::zoomToFit);
        toolbar->addWidget(zoomFitBtn);

        toolbar->addSpacing(8);

        // Marker button
        auto* markerBtn = new QPushButton("+ Marker");
        markerBtn->setObjectName("PodEditorMarkerBtn");
        markerBtn->setToolTip("Add marker at current position");
        connect(markerBtn, &QPushButton::clicked, this, [this]() {
            m_mod->addMarker(m_mod->position(), QString("M%1").arg(m_mod->markers().size() + 1));
        });
        toolbar->addWidget(markerBtn);

        toolbar->addStretch();

        // Position display
        m_posLabel = new QLabel("00:00.000 / 00:00.000");
        m_posLabel->setObjectName("PodEditorPosLabel");
        QFont pf = m_posLabel->font();
        pf.setPixelSize(12);
        pf.setFamily("Consolas");
        m_posLabel->setFont(pf);
        toolbar->addWidget(m_posLabel);

        root->addLayout(toolbar);

        // ── Timeline ruler ─────────────────────────────────────────────────
        m_ruler = new TimelineRuler(mod);
        root->addWidget(m_ruler);

        // ── Waveform view ──────────────────────────────────────────────────
        m_waveform = new WaveformView(mod);
        root->addWidget(m_waveform, 1);

        // ── Horizontal scroll bar ──────────────────────────────────────────
        m_scrollBar = new QScrollBar(Qt::Horizontal);
        m_scrollBar->setObjectName("PodEditorScrollBar");
        m_scrollBar->setRange(0, 1000);
        m_scrollBar->setValue(0);
        connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int v) {
            m_mod->setScrollPosition(v / 1000.0f);
            m_waveform->update();
            m_ruler->update();
        });
        root->addWidget(m_scrollBar);

        // ── Status bar ─────────────────────────────────────────────────────
        auto* statusRow = new QHBoxLayout;
        m_fileLabel = new QLabel("No file loaded");
        m_fileLabel->setObjectName("PodEditorFileLabel");
        statusRow->addWidget(m_fileLabel);
        statusRow->addStretch();

        m_zoomLabel = new QLabel("Zoom: 1.0x");
        m_zoomLabel->setObjectName("PodEditorZoomLabel");
        statusRow->addWidget(m_zoomLabel);
        root->addLayout(statusRow);

        // ── Connections ────────────────────────────────────────────────────
        connect(mod, &M1::PodEditorModule::fileLoaded, this, &PodEditorWidget::onFileLoaded);
        connect(mod, &M1::PodEditorModule::playbackStateChanged, this, &PodEditorWidget::onPlaybackChanged);
        connect(mod, &M1::PodEditorModule::positionChanged, this, &PodEditorWidget::onPositionChanged);
        connect(mod, &M1::PodEditorModule::zoomChanged, this, &PodEditorWidget::onZoomChanged);
        connect(mod, &M1::PodEditorModule::selectionChanged, this, [this]() {
            m_waveform->update();
        });
        connect(mod, &M1::PodEditorModule::markerAdded, this, [this]() {
            m_waveform->update();
        });
    }

private slots:
    void onOpen() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Open Audio File", {},
            "Audio Files (*.wav *.mp3 *.ogg *.flac *.opus *.m4a);;All Files (*)");
        if (!path.isEmpty())
            m_mod->loadFile(path);
    }

    void onFileLoaded(const QString& path) {
        m_fileLabel->setText(QFileInfo(path).fileName());
        m_waveform->update();
        m_ruler->update();
    }

    void onPlaybackChanged(bool playing) {
        m_playBtn->setText(playing ? "Pause" : "Play");
    }

    void onPositionChanged(qint64 ms) {
        const qint64 dur = m_mod->duration();
        m_posLabel->setText(QString("%1 / %2")
            .arg(formatTime(ms))
            .arg(formatTime(dur)));
        m_waveform->update();
    }

    void onZoomChanged(float level) {
        m_zoomLabel->setText(QString("Zoom: %1x").arg(level, 0, 'f', 1));
        m_waveform->update();
        m_ruler->update();
    }

private:
    static QString formatTime(qint64 ms) {
        const int totalSec = static_cast<int>(ms / 1000);
        const int min = totalSec / 60;
        const int sec = totalSec % 60;
        const int millis = static_cast<int>(ms % 1000);
        return QString("%1:%2.%3")
            .arg(min, 2, 10, QChar('0'))
            .arg(sec, 2, 10, QChar('0'))
            .arg(millis, 3, 10, QChar('0'));
    }

    M1::PodEditorModule* m_mod;
    QPushButton*    m_playBtn;
    QLabel*         m_posLabel;
    QLabel*         m_fileLabel;
    QLabel*         m_zoomLabel;
    WaveformView*   m_waveform;
    TimelineRuler*  m_ruler;
    QScrollBar*     m_scrollBar;
};

} // anonymous namespace

#include "PodEditorModule.moc"

namespace M1 {

// ─── PodEditorModule ─────────────────────────────────────────────────────────

PodEditorModule::PodEditorModule(QObject* parent) : IModule(parent) {}

PodEditorModule::~PodEditorModule() {
    delete m_player;
    delete m_audioOutput;
}

void PodEditorModule::initialize() {
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0f);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &PodEditorModule::onMediaStatusChanged);

    // Position ticker — updates UI at ~30fps
    m_posTicker = new QTimer(this);
    m_posTicker->setInterval(33);
    connect(m_posTicker, &QTimer::timeout, this, &PodEditorModule::onPositionTick);
}

void PodEditorModule::shutdown() {
    stop();
    if (m_posTicker) m_posTicker->stop();
}

QWidget* PodEditorModule::createWidget(QWidget* parent) {
    return new PodEditorWidget(this, parent);
}

// ─── File I/O ────────────────────────────────────────────────────────────────

void PodEditorModule::loadFile(const QString& path) {
    stop();
    m_filePath = path;
    m_markers.clear();
    m_selection = {};

    m_player->setSource(QUrl::fromLocalFile(path));

    // Generate placeholder waveform — in production this would analyze the file
    // with FFmpeg/libsndfile to extract actual peak data
    // For now we wait for duration from the player
    emit fileLoaded(path);
}

void PodEditorModule::onMediaStatusChanged() {
    if (m_player->mediaStatus() == QMediaPlayer::LoadedMedia ||
        m_player->mediaStatus() == QMediaPlayer::BufferedMedia) {
        const qint64 dur = m_player->duration();
        if (dur > 0 && m_waveform.durationMs != dur) {
            generatePlaceholderWaveform(dur);
            emit fileLoaded(m_filePath);
        }
    }
    if (m_player->mediaStatus() == QMediaPlayer::InvalidMedia) {
        emit editorError("Failed to load: " + m_filePath);
    }
}

void PodEditorModule::generatePlaceholderWaveform(qint64 durationMs) {
    m_waveform.durationMs = durationMs;
    m_waveform.sampleRate = 48000;

    // Generate a plausible-looking waveform with ~2 peaks per pixel at 700px
    const int peakCount = 1400;
    m_waveform.peaks.clear();
    m_waveform.peaks.reserve(peakCount);

    // Pseudo-random deterministic waveform based on duration
    unsigned int seed = static_cast<unsigned int>(durationMs) ^ 0xDEADBEEF;
    for (int i = 0; i < peakCount; ++i) {
        // Simple LCG for deterministic pseudo-random
        seed = seed * 1103515245 + 12345;
        const float r = (seed >> 16) / 65535.0f;

        // Shape: slightly louder in the middle, quieter at edges
        const float pos = static_cast<float>(i) / peakCount;
        const float envelope = 0.3f + 0.7f * (1.0f - 4.0f * (pos - 0.5f) * (pos - 0.5f));
        const float peak = std::clamp(r * envelope, 0.05f, 0.95f);
        m_waveform.peaks.append(peak);
    }
}

// ─── Playback ────────────────────────────────────────────────────────────────

void PodEditorModule::play() {
    if (!m_player || m_filePath.isEmpty()) return;
    m_player->play();
    m_playing.store(true, std::memory_order_relaxed);
    m_posTicker->start();
    emit playbackStateChanged(true);
}

void PodEditorModule::pause() {
    if (!m_player) return;
    m_player->pause();
    m_playing.store(false, std::memory_order_relaxed);
    m_posTicker->stop();
    emit playbackStateChanged(false);
}

void PodEditorModule::stop() {
    if (!m_player) return;
    m_player->stop();
    m_playing.store(false, std::memory_order_relaxed);
    if (m_posTicker) m_posTicker->stop();
    emit playbackStateChanged(false);
    emit positionChanged(0);
}

void PodEditorModule::seekTo(qint64 ms) {
    if (!m_player) return;
    m_player->setPosition(std::clamp(ms, qint64(0), m_waveform.durationMs));
    emit positionChanged(m_player->position());
}

qint64 PodEditorModule::position() const {
    if (!m_player) return 0;
    return m_player->position();
}

void PodEditorModule::onPositionTick() {
    if (m_player)
        emit positionChanged(m_player->position());
}

// ─── Selection ───────────────────────────────────────────────────────────────

void PodEditorModule::setSelection(qint64 startMs, qint64 endMs) {
    m_selection.startMs = std::max(qint64(0), startMs);
    m_selection.endMs   = std::min(endMs, m_waveform.durationMs);
    emit selectionChanged(m_selection.startMs, m_selection.endMs);
}

void PodEditorModule::clearSelection() {
    m_selection = {};
    emit selectionChanged(0, 0);
}

// ─── Edit operations (v1 stubs) ──────────────────────────────────────────────

void PodEditorModule::cut() {
    if (!hasSelection()) {
        emit editorError("No selection to cut");
        return;
    }
    m_clipboard = m_selection;
    emit editCut();
    emit statusChanged("Cut: " + QString("%1ms - %2ms")
        .arg(m_selection.startMs).arg(m_selection.endMs));
    // v1 stub: actual audio editing not implemented
}

void PodEditorModule::copy() {
    if (!hasSelection()) {
        emit editorError("No selection to copy");
        return;
    }
    m_clipboard = m_selection;
    emit editCopy();
    emit statusChanged("Copied: " + QString("%1ms - %2ms")
        .arg(m_selection.startMs).arg(m_selection.endMs));
}

void PodEditorModule::paste() {
    if (m_clipboard.endMs <= m_clipboard.startMs) {
        emit editorError("Nothing in clipboard");
        return;
    }
    emit editPaste();
    emit statusChanged("Paste at " + QString("%1ms").arg(position()));
    // v1 stub: actual audio editing not implemented
}

// ─── Markers ─────────────────────────────────────────────────────────────────

void PodEditorModule::addMarker(qint64 ms, const QString& label) {
    EditorMarker m;
    m.positionMs = std::clamp(ms, qint64(0), m_waveform.durationMs);
    m.label = label;
    m.color = QColor::fromHsl(static_cast<int>(m_markers.size() * 47 % 360), 200, 150);
    m_markers.append(m);
    emit markerAdded(m_markers.size() - 1);
}

void PodEditorModule::removeMarker(int index) {
    if (index >= 0 && index < m_markers.size()) {
        m_markers.removeAt(index);
        emit markerRemoved(index);
    }
}

// ─── Zoom ────────────────────────────────────────────────────────────────────

void PodEditorModule::zoomIn() {
    m_zoomLevel = std::min(m_zoomLevel * 1.5f, 64.0f);
    emit zoomChanged(m_zoomLevel);
}

void PodEditorModule::zoomOut() {
    m_zoomLevel = std::max(m_zoomLevel / 1.5f, 1.0f);
    if (m_zoomLevel <= 1.01f) {
        m_zoomLevel = 1.0f;
        m_scrollPos = 0.0f;
    }
    emit zoomChanged(m_zoomLevel);
}

void PodEditorModule::zoomToFit() {
    m_zoomLevel = 1.0f;
    m_scrollPos = 0.0f;
    emit zoomChanged(m_zoomLevel);
}

void PodEditorModule::setScrollPosition(float pos) {
    m_scrollPos = std::clamp(pos, 0.0f, 1.0f);
}

// ─── State persistence ───────────────────────────────────────────────────────

void PodEditorModule::saveState(QSettings& s) {
    s.setValue("filePath", m_filePath);
    s.setValue("zoomLevel", static_cast<double>(m_zoomLevel));

    // Save markers
    s.setValue("markerCount", m_markers.size());
    for (int i = 0; i < m_markers.size(); ++i) {
        const QString prefix = QString("marker%1/").arg(i);
        s.setValue(prefix + "posMs", m_markers[i].positionMs);
        s.setValue(prefix + "label", m_markers[i].label);
        s.setValue(prefix + "color", m_markers[i].color.name());
    }
}

void PodEditorModule::loadState(QSettings& s) {
    const QString path = s.value("filePath").toString();
    m_zoomLevel = static_cast<float>(s.value("zoomLevel", 1.0).toDouble());

    // Load markers
    const int markerCount = s.value("markerCount", 0).toInt();
    m_markers.clear();
    for (int i = 0; i < markerCount; ++i) {
        const QString prefix = QString("marker%1/").arg(i);
        EditorMarker m;
        m.positionMs = s.value(prefix + "posMs", 0).toLongLong();
        m.label      = s.value(prefix + "label").toString();
        m.color      = QColor(s.value(prefix + "color", "#0ea5e9").toString());
        m_markers.append(m);
    }

    // Re-load file if it existed
    if (!path.isEmpty() && QFile::exists(path))
        loadFile(path);
}

} // namespace M1
