/// @file   PodTranscribeModule.cpp
/// @path   Modules/PodTranscribeModule/PodTranscribeModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodTranscribe — Podcast Transcription Implementation
/// @purpose Implements the transcript segment model, speaker diarization
///          labels, editable transcript list widget, export to text/SRT/VTT,
///          and chapter marker generation. STT engine is stubbed for v1.
/// @reason  Podcast accessibility and show notes require text transcripts.
/// @changelog
///   2026-03-09  Initial implementation

#include "PodTranscribeModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QProgressBar>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <cmath>

namespace {

// ─── Time formatting helpers ────────────────────────────────────────────────
QString formatTimestamp(qint64 ms) {
    if (ms < 0) ms = 0;
    int totalSec = static_cast<int>(ms / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    int mmm = static_cast<int>(ms % 1000);
    if (h > 0)
        return QString("%1:%2:%3.%4")
            .arg(h).arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0')).arg(mmm / 100);
    return QString("%1:%2.%3")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')).arg(mmm / 100);
}

/// SRT timestamp: HH:MM:SS,mmm
QString srtTimestamp(qint64 ms) {
    int h = static_cast<int>(ms / 3600000);
    int m = static_cast<int>((ms % 3600000) / 60000);
    int s = static_cast<int>((ms % 60000) / 1000);
    int mmm = static_cast<int>(ms % 1000);
    return QString("%1:%2:%3,%4")
        .arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')).arg(mmm, 3, 10, QChar('0'));
}

/// VTT timestamp: HH:MM:SS.mmm
QString vttTimestamp(qint64 ms) {
    int h = static_cast<int>(ms / 3600000);
    int m = static_cast<int>((ms % 3600000) / 60000);
    int s = static_cast<int>((ms % 60000) / 1000);
    int mmm = static_cast<int>(ms % 1000);
    return QString("%1:%2:%3.%4")
        .arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')).arg(mmm, 3, 10, QChar('0'));
}

// ─── TranscriptTreeWidget — scrollable segment list ─────────────────────────
class TranscriptTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit TranscriptTreeWidget(M1::PodTranscribeModule* mod, QWidget* parent = nullptr)
        : QTreeWidget(parent), m_mod(mod)
    {
        setObjectName("PodTranscribeTree");
        setHeaderLabels({"Time", "Speaker", "Text", "Conf"});
        setRootIsDecorated(false);
        setAlternatingRowColors(true);
        setSelectionMode(QAbstractItemView::SingleSelection);

        header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        header()->setSectionResizeMode(2, QHeaderView::Stretch);
        header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

        setColumnWidth(0, 90);
        setColumnWidth(1, 80);
        setColumnWidth(3, 50);

        connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int col) {
            int idx = indexOfTopLevelItem(item);
            if (idx < 0) return;

            if (col == 0) {
                // Timestamp click -> emit position request
                auto segs = m_mod->segments();
                if (idx < segs.size()) {
                    emit m_mod->positionRequested(segs[idx].startMs);
                }
            } else if (col == 2) {
                // Edit text
                auto segs = m_mod->segments();
                if (idx >= segs.size()) return;
                bool ok = false;
                QString text = QInputDialog::getMultiLineText(this,
                    "Edit Segment", "Text:", segs[idx].text, &ok);
                if (ok) {
                    m_mod->editSegment(idx, text);
                }
            }
        });
    }

    void rebuild() {
        clear();
        auto segs = m_mod->segments();
        for (int i = 0; i < segs.size(); ++i) {
            const auto& seg = segs[i];
            auto* item = new QTreeWidgetItem;

            // Time range
            item->setText(0, QString("%1 - %2")
                .arg(formatTimestamp(seg.startMs),
                     formatTimestamp(seg.endMs)));
            item->setToolTip(0, "Double-click to seek to this position");

            // Speaker
            item->setText(1, seg.speaker);

            // Text
            item->setText(2, seg.text);

            // Confidence bar (as text percentage)
            if (seg.confidence > 0.0) {
                item->setText(3, QString("%1%").arg(
                    static_cast<int>(seg.confidence * 100)));
            } else {
                item->setText(3, "manual");
            }

            addTopLevelItem(item);
        }
    }

private:
    M1::PodTranscribeModule* m_mod;
};

// ─── ChapterListWidget — chapter marker list ────────────────────────────────
class ChapterListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit ChapterListWidget(M1::PodTranscribeModule* mod, QWidget* parent = nullptr)
        : QListWidget(parent), m_mod(mod)
    {
        setObjectName("PodTranscribeChapters");
        setMaximumHeight(100);
    }

    void rebuild() {
        clear();
        auto chapters = m_mod->chapterMarkers();
        for (const auto& ch : chapters) {
            addItem(QString("[%1] %2")
                .arg(formatTimestamp(ch.first), ch.second));
        }
    }

private:
    M1::PodTranscribeModule* m_mod;
};

// ─── PodTranscribeWidget — main composite widget ────────────────────────────
class PodTranscribeWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodTranscribeWidget(M1::PodTranscribeModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodTranscribeWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->setSpacing(4);

        // ── Toolbar ─────────────────────────────────────────────────────────
        auto* toolbar = new QHBoxLayout;
        toolbar->setSpacing(4);

        // Audio file selector
        auto* loadBtn = new QPushButton("Load Audio...");
        loadBtn->setObjectName("PodTranscribeLoad");
        loadBtn->setToolTip("Load audio file for transcription (Whisper integration - v2)");
        connect(loadBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this,
                "Load Audio for Transcription", {},
                "Audio Files (*.wav *.mp3 *.ogg *.flac *.opus *.m4a);;All Files (*)");
            if (!path.isEmpty()) {
                m_mod->loadAudioFile(path);
                m_audioLabel->setText(QFileInfo(path).fileName());
            }
        });
        toolbar->addWidget(loadBtn);

        m_audioLabel = new QLabel("No audio loaded");
        m_audioLabel->setObjectName("PodTranscribeAudioLabel");
        toolbar->addWidget(m_audioLabel, 1);

        // Add segment button
        auto* addSegBtn = new QPushButton("+ Segment");
        addSegBtn->setObjectName("PodTranscribeAddSeg");
        addSegBtn->setToolTip("Manually add a transcript segment");
        connect(addSegBtn, &QPushButton::clicked, this, &PodTranscribeWidget::onAddSegment);
        toolbar->addWidget(addSegBtn);

        // Remove segment button
        auto* removeSegBtn = new QPushButton("Remove");
        removeSegBtn->setObjectName("PodTranscribeRemoveSeg");
        removeSegBtn->setToolTip("Remove selected transcript segment");
        connect(removeSegBtn, &QPushButton::clicked, this, [this]() {
            auto* item = m_transcriptTree->currentItem();
            if (!item) return;
            int idx = m_transcriptTree->indexOfTopLevelItem(item);
            if (idx >= 0) {
                m_mod->removeSegment(idx);
            }
        });
        toolbar->addWidget(removeSegBtn);

        // Speaker management
        auto* speakerBtn = new QPushButton("Speakers...");
        speakerBtn->setObjectName("PodTranscribeSpeakerBtn");
        speakerBtn->setToolTip("Manage speaker labels");
        connect(speakerBtn, &QPushButton::clicked, this, &PodTranscribeWidget::onManageSpeakers);
        toolbar->addWidget(speakerBtn);

        // Export dropdown
        auto* exportBtn = new QPushButton("Export");
        exportBtn->setObjectName("PodTranscribeExportBtn");
        exportBtn->setToolTip("Export transcript");
        auto* exportMenu = new QMenu(exportBtn);
        auto* actText = exportMenu->addAction("Plain Text (.txt)");
        auto* actSrt  = exportMenu->addAction("SubRip (.srt)");
        auto* actVtt  = exportMenu->addAction("WebVTT (.vtt)");
        exportBtn->setMenu(exportMenu);
        connect(actText, &QAction::triggered, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export Transcript", {}, "Text Files (*.txt)");
            if (!path.isEmpty()) {
                if (m_mod->exportText(path))
                    m_statusLabel->setText("Exported to " + QFileInfo(path).fileName());
                else
                    m_statusLabel->setText("Export failed");
            }
        });
        connect(actSrt, &QAction::triggered, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export SRT", {}, "SRT Files (*.srt)");
            if (!path.isEmpty()) {
                if (m_mod->exportSrt(path))
                    m_statusLabel->setText("Exported SRT");
                else
                    m_statusLabel->setText("Export failed");
            }
        });
        connect(actVtt, &QAction::triggered, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export VTT", {}, "VTT Files (*.vtt)");
            if (!path.isEmpty()) {
                if (m_mod->exportVtt(path))
                    m_statusLabel->setText("Exported VTT");
                else
                    m_statusLabel->setText("Export failed");
            }
        });
        toolbar->addWidget(exportBtn);

        root->addLayout(toolbar);

        // ── Main content splitter ───────────────────────────────────────────
        auto* splitter = new QSplitter(Qt::Vertical);

        // Transcript tree
        m_transcriptTree = new TranscriptTreeWidget(mod);
        splitter->addWidget(m_transcriptTree);

        // Chapter markers panel
        auto* chapterGroup = new QGroupBox("Chapter Markers");
        chapterGroup->setObjectName("PodTranscribeChapterGroup");
        auto* chLay = new QVBoxLayout(chapterGroup);
        chLay->setContentsMargins(4, 4, 4, 4);

        m_chapterList = new ChapterListWidget(mod);
        chLay->addWidget(m_chapterList);

        auto* chBtnRow = new QHBoxLayout;
        auto* addChBtn = new QPushButton("+ Chapter");
        addChBtn->setObjectName("PodTranscribeAddChapter");
        addChBtn->setToolTip("Add a chapter marker at a specific timestamp");
        connect(addChBtn, &QPushButton::clicked, this, &PodTranscribeWidget::onAddChapter);
        chBtnRow->addWidget(addChBtn);

        auto* removeChBtn = new QPushButton("Remove");
        removeChBtn->setObjectName("PodTranscribeRemoveChapter");
        removeChBtn->setToolTip("Remove selected chapter marker");
        connect(removeChBtn, &QPushButton::clicked, this, [this]() {
            int row = m_chapterList->currentRow();
            if (row >= 0) m_mod->removeChapterMarker(row);
        });
        chBtnRow->addWidget(removeChBtn);

        auto* genChBtn = new QPushButton("Auto-Generate");
        genChBtn->setObjectName("PodTranscribeGenChapters");
        genChBtn->setToolTip("Generate chapters from transcript speaker changes");
        connect(genChBtn, &QPushButton::clicked, this, [this]() {
            auto chapters = m_mod->generateChapters();
            for (const auto& ch : chapters)
                m_mod->addChapterMarker(ch.first, ch.second);
            m_statusLabel->setText(QString("Generated %1 chapter(s)").arg(chapters.size()));
        });
        chBtnRow->addWidget(genChBtn);
        chBtnRow->addStretch();
        chLay->addLayout(chBtnRow);

        splitter->addWidget(chapterGroup);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);

        root->addWidget(splitter, 1);

        // ── Status bar ──────────────────────────────────────────────────────
        auto* statusRow = new QHBoxLayout;
        m_sttLabel = new QLabel("Whisper integration \xe2\x80\x94 v2 feature");
        m_sttLabel->setObjectName("PodTranscribeSttLabel");
        QFont sttFont;
        sttFont.setItalic(true);
        m_sttLabel->setFont(sttFont);
        statusRow->addWidget(m_sttLabel);
        statusRow->addStretch();

        m_statusLabel = new QLabel("Ready");
        m_statusLabel->setObjectName("PodTranscribeStatus");
        statusRow->addWidget(m_statusLabel);

        m_segCountLabel = new QLabel("0 segments");
        m_segCountLabel->setObjectName("PodTranscribeSegCount");
        statusRow->addWidget(m_segCountLabel);

        root->addLayout(statusRow);

        // ── Wire module signals ─────────────────────────────────────────────
        connect(mod, &M1::PodTranscribeModule::segmentsChanged, this, [this]() {
            m_transcriptTree->rebuild();
            m_segCountLabel->setText(QString("%1 segment(s)").arg(m_mod->segmentCount()));
        });
        connect(mod, &M1::PodTranscribeModule::chaptersChanged, this, [this]() {
            m_chapterList->rebuild();
        });
        connect(mod, &M1::PodTranscribeModule::audioFileChanged, this, [this](const QString& path) {
            m_audioLabel->setText(QFileInfo(path).fileName());
        });

        // Initial state
        if (!mod->audioFilePath().isEmpty())
            m_audioLabel->setText(QFileInfo(mod->audioFilePath()).fileName());
        m_transcriptTree->rebuild();
        m_chapterList->rebuild();
        m_segCountLabel->setText(QString("%1 segment(s)").arg(mod->segmentCount()));
    }

private slots:
    void onAddSegment() {
        // Get start time
        bool ok = false;
        int startSec = QInputDialog::getInt(this, "Add Segment",
            "Start time (seconds):", 0, 0, 999999, 1, &ok);
        if (!ok) return;

        int endSec = QInputDialog::getInt(this, "Add Segment",
            "End time (seconds):", startSec + 5, startSec, 999999, 1, &ok);
        if (!ok) return;

        // Speaker selection
        QStringList speakers = m_mod->availableSpeakers();
        if (speakers.isEmpty()) speakers << "Host";
        QString speaker = QInputDialog::getItem(this, "Add Segment",
            "Speaker:", speakers, 0, true, &ok);
        if (!ok) return;

        QString text = QInputDialog::getMultiLineText(this,
            "Add Segment", "Transcribed text:", {}, &ok);
        if (!ok || text.isEmpty()) return;

        m_mod->addSegment(
            static_cast<qint64>(startSec) * 1000,
            static_cast<qint64>(endSec) * 1000,
            speaker, text);
    }

    void onManageSpeakers() {
        QStringList speakers = m_mod->availableSpeakers();
        bool ok = false;
        QString name = QInputDialog::getText(this, "Add Speaker",
            QString("Current speakers: %1\n\nAdd new speaker:")
                .arg(speakers.isEmpty() ? "none" : speakers.join(", ")),
            QLineEdit::Normal, {}, &ok);
        if (ok && !name.isEmpty()) {
            m_mod->addSpeaker(name);
            m_statusLabel->setText("Added speaker: " + name);
        }
    }

    void onAddChapter() {
        bool ok = false;
        int sec = QInputDialog::getInt(this, "Add Chapter",
            "Timestamp (seconds):", 0, 0, 999999, 1, &ok);
        if (!ok) return;

        QString title = QInputDialog::getText(this, "Add Chapter",
            "Chapter title:", QLineEdit::Normal, {}, &ok);
        if (!ok || title.isEmpty()) return;

        m_mod->addChapterMarker(static_cast<qint64>(sec) * 1000, title);
    }

private:
    M1::PodTranscribeModule* m_mod;
    QLabel*               m_audioLabel      = nullptr;
    TranscriptTreeWidget* m_transcriptTree  = nullptr;
    ChapterListWidget*    m_chapterList     = nullptr;
    QLabel*               m_sttLabel        = nullptr;
    QLabel*               m_statusLabel     = nullptr;
    QLabel*               m_segCountLabel   = nullptr;
};

} // anonymous namespace

#include "PodTranscribeModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
PodTranscribeModule::PodTranscribeModule(QObject* parent)
    : IModule(parent)
{
    // Default speaker labels
    m_speakers << "Host" << "Guest 1" << "Guest 2" << "Guest 3";
}

PodTranscribeModule::~PodTranscribeModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void PodTranscribeModule::initialize() {
    // STT engine initialization would go here in v2
}

void PodTranscribeModule::shutdown() {
    // STT engine cleanup would go here in v2
}

QWidget* PodTranscribeModule::createWidget(QWidget* parent) {
    return new PodTranscribeWidget(this, parent);
}

// ─── Audio file ─────────────────────────────────────────────────────────────
void PodTranscribeModule::loadAudioFile(const QString& path) {
    if (m_audioPath != path) {
        m_audioPath = path;
        emit audioFileChanged(path);
        // In v2, this would trigger the Whisper STT pipeline:
        //   1. Load audio via FFmpeg
        //   2. Feed to Whisper model
        //   3. Populate m_segments with timestamped results
        //   4. Emit segmentsChanged()
        qInfo() << "[PodTranscribeModule] Audio file loaded:" << path
                << "(STT not yet available — manual entry only)";
    }
}

// ─── Segment management ────────────────────────────────────────────────────
void PodTranscribeModule::addSegment(qint64 startMs, qint64 endMs,
                                      const QString& speaker, const QString& text) {
    TranscriptSegment seg;
    seg.startMs    = startMs;
    seg.endMs      = endMs;
    seg.speaker    = speaker;
    seg.text       = text;
    seg.confidence = 0.0;  // manual entry

    // Insert in chronological order
    int insertIdx = m_segments.size();
    for (int i = 0; i < m_segments.size(); ++i) {
        if (startMs < m_segments[i].startMs) {
            insertIdx = i;
            break;
        }
    }
    m_segments.insert(insertIdx, seg);

    // Add speaker if new
    if (!m_speakers.contains(speaker) && !speaker.isEmpty()) {
        m_speakers.append(speaker);
        emit speakersChanged();
    }

    emit segmentsChanged();
}

void PodTranscribeModule::editSegment(int index, const QString& text) {
    if (index >= 0 && index < m_segments.size()) {
        m_segments[index].text = text;
        emit segmentsChanged();
    }
}

void PodTranscribeModule::removeSegment(int index) {
    if (index >= 0 && index < m_segments.size()) {
        m_segments.removeAt(index);
        emit segmentsChanged();
    }
}

void PodTranscribeModule::clearTranscript() {
    m_segments.clear();
    emit segmentsChanged();
}

// ─── Speaker management ────────────────────────────────────────────────────
void PodTranscribeModule::setSpeakerLabel(int index, const QString& label) {
    if (index >= 0 && index < m_segments.size()) {
        m_segments[index].speaker = label;
        if (!m_speakers.contains(label) && !label.isEmpty()) {
            m_speakers.append(label);
            emit speakersChanged();
        }
        emit segmentsChanged();
    }
}

void PodTranscribeModule::addSpeaker(const QString& name) {
    if (!m_speakers.contains(name) && !name.isEmpty()) {
        m_speakers.append(name);
        emit speakersChanged();
    }
}

void PodTranscribeModule::removeSpeaker(const QString& name) {
    m_speakers.removeAll(name);
    emit speakersChanged();
}

// ─── Export ─────────────────────────────────────────────────────────────────
bool PodTranscribeModule::exportText(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFinished(false, path);
        return false;
    }

    QTextStream ts(&file);
    ts << "Podcast Transcript\n";
    ts << "==================\n\n";

    if (!m_audioPath.isEmpty())
        ts << "Source: " << QFileInfo(m_audioPath).fileName() << "\n\n";

    for (const auto& seg : m_segments) {
        ts << "[" << formatTimestamp(seg.startMs) << " - "
           << formatTimestamp(seg.endMs) << "] ";
        if (!seg.speaker.isEmpty())
            ts << seg.speaker << ": ";
        ts << seg.text << "\n\n";
    }

    file.close();
    emit exportFinished(true, path);
    return true;
}

bool PodTranscribeModule::exportSrt(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFinished(false, path);
        return false;
    }

    QTextStream ts(&file);
    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        ts << (i + 1) << "\n";
        ts << srtTimestamp(seg.startMs) << " --> " << srtTimestamp(seg.endMs) << "\n";
        if (!seg.speaker.isEmpty())
            ts << "<v " << seg.speaker << ">";
        ts << seg.text << "\n\n";
    }

    file.close();
    emit exportFinished(true, path);
    return true;
}

bool PodTranscribeModule::exportVtt(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFinished(false, path);
        return false;
    }

    QTextStream ts(&file);
    ts << "WEBVTT\n\n";

    for (int i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        ts << (i + 1) << "\n";
        ts << vttTimestamp(seg.startMs) << " --> " << vttTimestamp(seg.endMs) << "\n";
        if (!seg.speaker.isEmpty())
            ts << "<v " << seg.speaker << ">";
        ts << seg.text << "\n\n";
    }

    file.close();
    emit exportFinished(true, path);
    return true;
}

// ─── Chapter generation ────────────────────────────────────────────────────
QList<QPair<qint64, QString>> PodTranscribeModule::generateChapters() const {
    // Generate chapter markers from speaker changes in the transcript.
    // Each time the speaker changes, a new chapter is created using the
    // first few words of the segment as the title.
    QList<QPair<qint64, QString>> chapters;
    if (m_segments.isEmpty()) return chapters;

    QString lastSpeaker;
    for (const auto& seg : m_segments) {
        if (seg.speaker != lastSpeaker) {
            // New speaker = new chapter
            QString title = seg.speaker;
            // Append first ~30 chars of text as subtitle
            if (!seg.text.isEmpty()) {
                QString preview = seg.text.left(30);
                if (seg.text.length() > 30) preview += "...";
                title += ": " + preview;
            }
            chapters.append({seg.startMs, title});
            lastSpeaker = seg.speaker;
        }
    }
    return chapters;
}

void PodTranscribeModule::addChapterMarker(qint64 timestampMs, const QString& title) {
    // Insert in chronological order
    int insertIdx = m_chapters.size();
    for (int i = 0; i < m_chapters.size(); ++i) {
        if (timestampMs < m_chapters[i].first) {
            insertIdx = i;
            break;
        }
    }
    m_chapters.insert(insertIdx, {timestampMs, title});
    emit chaptersChanged();
}

void PodTranscribeModule::removeChapterMarker(int index) {
    if (index >= 0 && index < m_chapters.size()) {
        m_chapters.removeAt(index);
        emit chaptersChanged();
    }
}

// ─── State persistence ──────────────────────────────────────────────────────
void PodTranscribeModule::saveState(QSettings& s) {
    s.beginGroup("PodTranscribe");

    s.setValue("audioPath", m_audioPath);

    // Speakers
    s.setValue("speakers", m_speakers);

    // Segments
    s.setValue("segmentCount", m_segments.size());
    for (int i = 0; i < m_segments.size(); ++i) {
        s.beginGroup(QString("seg_%1").arg(i));
        s.setValue("startMs",    m_segments[i].startMs);
        s.setValue("endMs",      m_segments[i].endMs);
        s.setValue("speaker",    m_segments[i].speaker);
        s.setValue("text",       m_segments[i].text);
        s.setValue("confidence", m_segments[i].confidence);
        s.endGroup();
    }

    // Chapters
    s.setValue("chapterCount", m_chapters.size());
    for (int i = 0; i < m_chapters.size(); ++i) {
        s.beginGroup(QString("ch_%1").arg(i));
        s.setValue("timestampMs", m_chapters[i].first);
        s.setValue("title",       m_chapters[i].second);
        s.endGroup();
    }

    s.endGroup();
}

void PodTranscribeModule::loadState(QSettings& s) {
    s.beginGroup("PodTranscribe");

    m_audioPath = s.value("audioPath").toString();

    // Speakers
    m_speakers = s.value("speakers").toStringList();
    if (m_speakers.isEmpty())
        m_speakers << "Host" << "Guest 1" << "Guest 2" << "Guest 3";

    // Segments
    m_segments.clear();
    int segCount = s.value("segmentCount", 0).toInt();
    for (int i = 0; i < segCount; ++i) {
        s.beginGroup(QString("seg_%1").arg(i));
        TranscriptSegment seg;
        seg.startMs    = s.value("startMs", 0).toLongLong();
        seg.endMs      = s.value("endMs", 0).toLongLong();
        seg.speaker    = s.value("speaker").toString();
        seg.text       = s.value("text").toString();
        seg.confidence = s.value("confidence", 0.0).toDouble();
        m_segments.append(seg);
        s.endGroup();
    }

    // Chapters
    m_chapters.clear();
    int chCount = s.value("chapterCount", 0).toInt();
    for (int i = 0; i < chCount; ++i) {
        s.beginGroup(QString("ch_%1").arg(i));
        qint64 ts = s.value("timestampMs", 0).toLongLong();
        QString title = s.value("title").toString();
        m_chapters.append({ts, title});
        s.endGroup();
    }

    s.endGroup();
}

} // namespace M1
