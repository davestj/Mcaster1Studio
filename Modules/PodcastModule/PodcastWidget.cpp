#include "PodcastWidget.h"
#include "PodcastModule.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QDebug>
#include <cmath>

// ─── Button style helpers ─────────────────────────────────────────────────────
// Transport buttons keep semantic colors (red/amber/blue/gray) in all themes.
// Only the disabled state and border adapt to the theme.
static QString btnStyle(const QString& bg) {
    const auto p = ThemePalette::forCurrentTheme();
    return QString(
        "QPushButton {"
        "  background: %1;"
        "  color: #ffffff;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  padding: 6px 14px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:disabled { background: %3; color: %4; }"
    ).arg(bg, p.border.name(), p.panelBg.name(), p.textMuted.name());
}

// ─── TrackStrip ───────────────────────────────────────────────────────────────

TrackStrip::TrackStrip(int trackIndex, M1::PodcastModule* module, QWidget* parent)
    : QWidget(parent)
    , m_trackIndex(trackIndex)
    , m_module(module)
{
    setMinimumHeight(32);
    const auto p = ThemePalette::forCurrentTheme();
    setStyleSheet(QString(
        "TrackStrip { background: %1; border: 1px solid %2; border-radius: 4px; }"
    ).arg(p.panelBg.name(), p.border.name()));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(8);

    m_nameLabel = new QLabel(module->trackName(trackIndex), this);
    m_nameLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(p.text.name()));
    m_nameLabel->setFixedWidth(64);
    row->addWidget(m_nameLabel);

    m_levelMeter = new QProgressBar(this);
    m_levelMeter->setRange(0, 100);
    m_levelMeter->setValue(0);
    m_levelMeter->setTextVisible(false);
    m_levelMeter->setStyleSheet(QString(
        "QProgressBar {"
        "  border: 1px solid %1;"
        "  border-radius: 3px;"
        "  background: %2;"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 2px;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 %3, stop:0.65 %3,"
        "    stop:0.80 %4, stop:0.92 %5, stop:1 %5);"
        "}"
    ).arg(p.border.name(), p.inputBg.name(),
          p.vuGreen.name(), p.vuYellow.name(), p.vuRed.name()));
    row->addWidget(m_levelMeter, 1);
}

void TrackStrip::pollLevel() {
    if (!m_module || !m_levelMeter) return;
    const float level = m_module->trackLevel(m_trackIndex);
    const int pct = static_cast<int>(std::min(1.0f, level) * 100.0f);
    m_levelMeter->setValue(pct);
}

// ─── PodcastWidget ────────────────────────────────────────────────────────────

PodcastWidget::PodcastWidget(M1::PodcastModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    buildUi();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    // Connect module signals
    connect(module, &M1::PodcastModule::stateChanged,
            this,   &PodcastWidget::onStateChanged, Qt::QueuedConnection);
    connect(module, &M1::PodcastModule::exportFinished,
            this,   &PodcastWidget::onExportFinished, Qt::QueuedConnection);
    connect(module, &M1::PodcastModule::recordingError,
            this,   [this](const QString& msg) {
                QMessageBox::warning(this, "Recording Error", msg);
            }, Qt::QueuedConnection);

    // Meter polling
    connect(&m_meterTimer, &QTimer::timeout, this, &PodcastWidget::pollMeters);
    m_meterTimer.start(50);

    // Time display
    connect(&m_timeTimer, &QTimer::timeout, this, &PodcastWidget::updateTimeDisplay);
    m_timeTimer.start(100);

    updateTransportButtons(module->sessionState());
}

void PodcastWidget::applyTheme() {
    const auto p = ThemePalette::forCurrentTheme();
    setStyleSheet(QString(R"(
        PodcastWidget { background: %1; }
        QGroupBox {
            color: %3; border: 1px solid %2; border-radius: 4px;
            margin-top: 8px; padding-top: 8px; font-size: 12px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; top: 1px; }
        QLabel { color: %3; font-family: 'Segoe UI'; }
        QLabel[objectName="MutedLabel"] { color: %4; font-size: 12px; }
        QLineEdit {
            background: %5; color: %3; border: 1px solid %2;
            border-radius: 3px; padding: 4px;
        }
        QListWidget {
            background: %5; color: %3; border: 1px solid %2; border-radius: 3px;
        }
        QListWidget::item:selected { background: %2; }
    )").arg(p.bg.name(), p.border.name(), p.text.name(),
            p.textMuted.name(), p.inputBg.name()));

    // Update TrackStrip backgrounds (they style themselves in their own constructor)
    for (auto* strip : m_tracks) {
        if (strip) {
            strip->setStyleSheet(QString(
                "TrackStrip { background: %1; border: 1px solid %2; border-radius: 4px; }"
            ).arg(p.panelBg.name(), p.border.name()));
        }
    }
}

void PodcastWidget::buildUi() {
    setObjectName("PodcastWidget");
    applyTheme();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ── 1. Time display ───────────────────────────────────────────────────
    {
        auto* timeRow = new QHBoxLayout;
        timeRow->addStretch(1);

        m_timeLabel = new QLabel("00:00:00.000", this);
        QFont f("Courier New", 20, QFont::Bold);
        m_timeLabel->setFont(f);
        {
            const auto tp = ThemePalette::forCurrentTheme();
            m_timeLabel->setStyleSheet(QString("color: %1; letter-spacing: 2px;")
                .arg(tp.success.name()));
        }
        timeRow->addWidget(m_timeLabel);

        timeRow->addStretch(1);
        root->addLayout(timeRow);
    }

    // ── 2. Transport bar ──────────────────────────────────────────────────
    {
        auto* transGroup = new QGroupBox("Transport", this);
        auto* transRow   = new QHBoxLayout(transGroup);
        transRow->setSpacing(6);

        const auto tp = ThemePalette::forCurrentTheme();

        m_armBtn = new QPushButton("ARM", this);
        m_armBtn->setStyleSheet(btnStyle(tp.info.name()));
        m_armBtn->setToolTip("Arm the recorder — goes to standby");
        transRow->addWidget(m_armBtn);

        m_recBtn = new QPushButton("REC", this);
        m_recBtn->setStyleSheet(btnStyle(tp.error.name()));
        m_recBtn->setToolTip("Start / resume recording");
        transRow->addWidget(m_recBtn);

        m_pauseBtn = new QPushButton("PAUSE", this);
        m_pauseBtn->setStyleSheet(btnStyle(tp.warning.name()));
        m_pauseBtn->setToolTip("Pause recording");
        transRow->addWidget(m_pauseBtn);

        m_stopBtn = new QPushButton("STOP", this);
        m_stopBtn->setStyleSheet(btnStyle(tp.textMuted.name()));
        m_stopBtn->setToolTip("Stop and finalize recording");
        transRow->addWidget(m_stopBtn);

        transRow->addStretch(1);

        connect(m_armBtn,   &QPushButton::clicked, m_module, &M1::PodcastModule::arm);
        connect(m_recBtn,   &QPushButton::clicked, m_module, &M1::PodcastModule::record);
        connect(m_pauseBtn, &QPushButton::clicked, m_module, &M1::PodcastModule::pause);
        connect(m_stopBtn,  &QPushButton::clicked, m_module, &M1::PodcastModule::stop);

        root->addWidget(transGroup);
    }

    // ── 3. Track lanes ────────────────────────────────────────────────────
    {
        auto* tracksGroup = new QGroupBox("Tracks", this);
        auto* tracksLayout = new QVBoxLayout(tracksGroup);
        tracksLayout->setSpacing(4);

        m_tracks.clear();
        for (int i = 0; i < M1::PodcastModule::kMaxTracks; ++i) {
            auto* strip = new TrackStrip(i, m_module, tracksGroup);
            m_tracks.append(strip);
            tracksLayout->addWidget(strip);
        }

        root->addWidget(tracksGroup);
    }

    // ── 4. Chapter panel ──────────────────────────────────────────────────
    {
        auto* chapGroup  = new QGroupBox("Chapters", this);
        auto* chapLayout = new QVBoxLayout(chapGroup);
        chapLayout->setSpacing(4);

        m_chapterList = new QListWidget(chapGroup);
        m_chapterList->setMinimumHeight(60);
        chapLayout->addWidget(m_chapterList);

        auto* chapBtnRow = new QHBoxLayout;
        m_addChapBtn = new QPushButton("Add Chapter at Current Time", chapGroup);
        { const auto p2 = ThemePalette::forCurrentTheme();
          m_addChapBtn->setStyleSheet(btnStyle(p2.accent.name())); }
        chapBtnRow->addWidget(m_addChapBtn);

        m_delChapBtn = new QPushButton("Remove Selected", chapGroup);
        { const auto p2 = ThemePalette::forCurrentTheme();
          m_delChapBtn->setStyleSheet(btnStyle(p2.textMuted.name())); }
        chapBtnRow->addWidget(m_delChapBtn);

        chapBtnRow->addStretch(1);
        chapLayout->addLayout(chapBtnRow);

        connect(m_addChapBtn, &QPushButton::clicked, this, &PodcastWidget::onAddChapter);
        connect(m_delChapBtn, &QPushButton::clicked, this, &PodcastWidget::onRemoveChapter);

        root->addWidget(chapGroup);
    }

    // ── 5. Export bar ─────────────────────────────────────────────────────
    {
        auto* expGroup  = new QGroupBox("Export", this);
        auto* expLayout = new QVBoxLayout(expGroup);
        expLayout->setSpacing(4);

        auto* pathRow = new QHBoxLayout;
        auto* pathLbl = new QLabel("File:", expGroup);
        pathLbl->setObjectName("MutedLabel");
        pathRow->addWidget(pathLbl);

        m_exportPath = new QLineEdit(expGroup);
        m_exportPath->setPlaceholderText("Select output file path...");
        pathRow->addWidget(m_exportPath, 1);

        m_browseBtn = new QPushButton("Browse", expGroup);
        { const auto p2 = ThemePalette::forCurrentTheme();
          m_browseBtn->setStyleSheet(btnStyle(p2.textMuted.name())); }
        m_browseBtn->setMinimumWidth(60);
        pathRow->addWidget(m_browseBtn);

        expLayout->addLayout(pathRow);

        auto* actionRow = new QHBoxLayout;

        m_exportBtn = new QPushButton("Export WAV", expGroup);
        { const auto p2 = ThemePalette::forCurrentTheme();
          m_exportBtn->setStyleSheet(btnStyle(p2.success.name())); }
        actionRow->addWidget(m_exportBtn);

        m_exportStatus = new QLabel("", expGroup);
        m_exportStatus->setObjectName("MutedLabel");
        actionRow->addWidget(m_exportStatus, 1);

        expLayout->addLayout(actionRow);

        connect(m_browseBtn,  &QPushButton::clicked, this, &PodcastWidget::onBrowseExport);
        connect(m_exportBtn,  &QPushButton::clicked, this, &PodcastWidget::onExportWav);

        root->addWidget(expGroup);
    }

    root->addStretch(1);
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void PodcastWidget::onStateChanged(M1::PodcastModule::State newState) {
    updateTransportButtons(newState);
}

void PodcastWidget::updateTransportButtons(M1::PodcastModule::State s) {
    using State = M1::PodcastModule::State;

    if (!m_armBtn) return;

    m_armBtn->setEnabled(  s == State::Idle);
    m_recBtn->setEnabled(  s == State::Armed || s == State::Paused);
    m_pauseBtn->setEnabled(s == State::Recording);
    m_stopBtn->setEnabled( s == State::Recording || s == State::Paused || s == State::Armed);

    // Highlight active state button
    const auto tp = ThemePalette::forCurrentTheme();
    const bool isRec = (s == State::Recording);
    m_recBtn->setStyleSheet(isRec
        ? btnStyle(tp.error.name()) + "QPushButton { border: 2px solid " + tp.error.lighter(150).name() + "; }"
        : btnStyle(tp.error.name()));
}

void PodcastWidget::onAddChapter() {
    using State = M1::PodcastModule::State;
    const auto s = m_module->sessionState();
    if (s != State::Recording && s != State::Paused) {
        QMessageBox::information(this, "Add Chapter",
                                 "Start recording before adding chapters.");
        return;
    }

    bool ok = false;
    const QString title = QInputDialog::getText(this, "Add Chapter",
                                                "Chapter title:", QLineEdit::Normal,
                                                QString(), &ok);
    if (ok) {
        m_module->addChapter(title);
        refreshChapterList();
    }
}

void PodcastWidget::onRemoveChapter() {
    if (!m_chapterList) return;
    const int row = m_chapterList->currentRow();
    if (row < 0) return;
    m_module->removeChapter(row);
    refreshChapterList();
}

void PodcastWidget::refreshChapterList() {
    if (!m_chapterList) return;
    m_chapterList->clear();
    const auto& chapters = m_module->chapters();
    for (const auto& ch : chapters) {
        const qint64 ms = ch.positionMs;
        const int hh = static_cast<int>(ms / 3600000);
        const int mm = static_cast<int>((ms % 3600000) / 60000);
        const int ss = static_cast<int>((ms % 60000) / 1000);
        const int mmm = static_cast<int>(ms % 1000);
        const QString ts = QString("%1:%2:%3.%4")
            .arg(hh, 2, 10, QChar('0'))
            .arg(mm, 2, 10, QChar('0'))
            .arg(ss, 2, 10, QChar('0'))
            .arg(mmm, 3, 10, QChar('0'));
        m_chapterList->addItem(QString("[%1] %2").arg(ts, ch.title));
    }
}

void PodcastWidget::onBrowseExport() {
    const QString path = QFileDialog::getSaveFileName(this,
        "Export Podcast", QString(), "WAV Audio (*.wav);;All Files (*.*)");
    if (!path.isEmpty()) {
        m_exportPath->setText(path);
    }
}

void PodcastWidget::onExportWav() {
    const QString path = m_exportPath->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Export", "Please select an output file path.");
        return;
    }
    m_exportStatus->setText("Exporting...");
    m_exportBtn->setEnabled(false);

    QString err;
    const bool ok = m_module->exportWav(path, err);
    if (!ok) {
        m_exportStatus->setText("Export failed.");
        QMessageBox::critical(this, "Export Failed", err);
        m_exportBtn->setEnabled(true);
    }
    // onExportFinished handles the success case via signal
}

void PodcastWidget::onExportFinished(bool success, const QString& path) {
    m_exportBtn->setEnabled(true);
    const auto tp = ThemePalette::forCurrentTheme();
    if (success) {
        m_exportStatus->setStyleSheet(QString("color: %1; font-size: 12px;").arg(tp.success.name()));
        m_exportStatus->setText(QString("Exported: %1").arg(path));
    } else {
        m_exportStatus->setStyleSheet(QString("color: %1; font-size: 12px;").arg(tp.error.name()));
        m_exportStatus->setText("Export failed — see log.");
    }
}

void PodcastWidget::pollMeters() {
    for (auto* strip : m_tracks) {
        strip->pollLevel();
    }
}

void PodcastWidget::updateTimeDisplay() {
    if (!m_timeLabel || !m_module) return;

    const qint64 ms = m_module->elapsedMs();
    const int hh    = static_cast<int>(ms / 3600000);
    const int mm    = static_cast<int>((ms % 3600000) / 60000);
    const int ss    = static_cast<int>((ms % 60000) / 1000);
    const int mmm   = static_cast<int>(ms % 1000);

    m_timeLabel->setText(QString("%1:%2:%3.%4")
        .arg(hh,  2, 10, QChar('0'))
        .arg(mm,  2, 10, QChar('0'))
        .arg(ss,  2, 10, QChar('0'))
        .arg(mmm, 3, 10, QChar('0')));

    // Flash red during recording
    const auto state = m_module->sessionState();
    const auto tp = ThemePalette::forCurrentTheme();
    if (state == M1::PodcastModule::State::Recording) {
        static bool flash = false;
        flash = !flash;
        m_timeLabel->setStyleSheet(QString("color: %1; letter-spacing: 2px;")
            .arg(flash ? tp.error.name() : tp.error.lighter(140).name()));
    } else {
        m_timeLabel->setStyleSheet(QString("color: %1; letter-spacing: 2px;")
            .arg(tp.success.name()));
    }
}
