/// @file   TranscribeRecModule.cpp
/// @path   Modules/TranscribeRecModule/TranscribeRecModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TranscribeRec — Sermon Recording and Transcription Implementation
/// @purpose Implements sermon audio capture from the PTT mic pipeline, WAV
///          recording, transcript editing interface, and export to text/SRT.
///          Speech-to-text is stubbed for v1 — future integration with
///          Whisper or cloud STT planned.
/// @reason  Churches need sermon recordings for web publishing, podcasts,
///          and accessibility compliance.
/// @changelog
///   2026-03-09  Initial implementation

#include "TranscribeRecModule.h"
#include "PTTModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTemporaryFile>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QDateTime>
#include <QSplitter>
#include <cmath>

namespace {

// ─── Time formatting helper ─────────────────────────────────────────────────
QString formatTime(qint64 ms) {
    if (ms < 0) ms = 0;
    int totalSec = static_cast<int>(ms / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    return QString("%1:%2:%3")
        .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

/// SRT timestamp format: HH:MM:SS,mmm
QString srtTimestamp(qint64 ms) {
    int h = static_cast<int>(ms / 3600000);
    int m = static_cast<int>((ms % 3600000) / 60000);
    int s = static_cast<int>((ms % 60000) / 1000);
    int mmm = static_cast<int>(ms % 1000);
    return QString("%1:%2:%3,%4")
        .arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')).arg(mmm, 3, 10, QChar('0'));
}

// ─── RecordingPanel — transport controls + level meter ──────────────────────
class RecordingPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecordingPanel(M1::TranscribeRecModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("TranscribeRecRecording");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        // Sermon metadata
        auto* metaGroup = new QGroupBox("Sermon Info");
        metaGroup->setObjectName("TranscribeRecMeta");
        auto* metaLay = new QGridLayout(metaGroup);
        metaLay->addWidget(new QLabel("Title:"), 0, 0);
        m_titleEdit = new QLineEdit;
        m_titleEdit->setObjectName("TranscribeRecTitle");
        m_titleEdit->setPlaceholderText("Sermon title...");
        metaLay->addWidget(m_titleEdit, 0, 1);
        metaLay->addWidget(new QLabel("Speaker:"), 1, 0);
        m_speakerEdit = new QLineEdit;
        m_speakerEdit->setObjectName("TranscribeRecSpeaker");
        m_speakerEdit->setPlaceholderText("Pastor name...");
        metaLay->addWidget(m_speakerEdit, 1, 1);
        lay->addWidget(metaGroup);

        connect(m_titleEdit, &QLineEdit::textChanged, mod, &M1::TranscribeRecModule::setSermonTitle);
        connect(m_speakerEdit, &QLineEdit::textChanged, mod, &M1::TranscribeRecModule::setSpeaker);

        // Level meter
        m_levelBar = new QProgressBar;
        m_levelBar->setObjectName("TranscribeRecLevel");
        m_levelBar->setRange(0, 100);
        m_levelBar->setTextVisible(false);
        m_levelBar->setMaximumHeight(12);
        lay->addWidget(m_levelBar);

        // Recording time display
        m_timeLabel = new QLabel("00:00:00");
        m_timeLabel->setObjectName("TranscribeRecTime");
        m_timeLabel->setAlignment(Qt::AlignCenter);
        QFont timeFont;
        timeFont.setPointSize(24);
        timeFont.setBold(true);
        m_timeLabel->setFont(timeFont);
        lay->addWidget(m_timeLabel);

        // State indicator
        m_stateLabel = new QLabel("IDLE");
        m_stateLabel->setObjectName("TranscribeRecState");
        m_stateLabel->setAlignment(Qt::AlignCenter);
        lay->addWidget(m_stateLabel);

        // Transport buttons
        auto* btnRow = new QHBoxLayout;
        m_recBtn = new QPushButton("Record");
        m_recBtn->setObjectName("TranscribeRecRecBtn");
        m_pauseBtn = new QPushButton("Pause");
        m_pauseBtn->setObjectName("TranscribeRecPauseBtn");
        m_pauseBtn->setEnabled(false);
        m_stopBtn = new QPushButton("Stop");
        m_stopBtn->setObjectName("TranscribeRecStopBtn");
        m_stopBtn->setEnabled(false);
        btnRow->addWidget(m_recBtn);
        btnRow->addWidget(m_pauseBtn);
        btnRow->addWidget(m_stopBtn);
        lay->addLayout(btnRow);

        // Export buttons
        auto* exportRow = new QHBoxLayout;
        auto* exportWavBtn = new QPushButton("Export WAV");
        exportWavBtn->setObjectName("TranscribeRecExportWav");
        auto* exportTxtBtn = new QPushButton("Export Text");
        exportTxtBtn->setObjectName("TranscribeRecExportTxt");
        auto* exportSrtBtn = new QPushButton("Export SRT");
        exportSrtBtn->setObjectName("TranscribeRecExportSrt");
        exportRow->addWidget(exportWavBtn);
        exportRow->addWidget(exportTxtBtn);
        exportRow->addWidget(exportSrtBtn);
        lay->addLayout(exportRow);

        // Connections
        connect(m_recBtn, &QPushButton::clicked, this, [this]() {
            auto state = m_mod->recordingState();
            if (state == M1::TranscribeRecModule::State::Idle) {
                m_mod->startRecording();
            } else if (state == M1::TranscribeRecModule::State::Paused) {
                m_mod->resumeRecording();
            }
        });
        connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
            m_mod->pauseRecording();
        });
        connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
            m_mod->stopRecording();
        });
        connect(exportWavBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export WAV", generateFilename(".wav"), "WAV Files (*.wav)");
            if (path.isEmpty()) return;
            QString error;
            if (!m_mod->exportWav(path, error))
                QMessageBox::warning(this, "Export Error", error);
        });
        connect(exportTxtBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export Transcript", generateFilename(".txt"), "Text Files (*.txt)");
            if (!path.isEmpty()) m_mod->exportTranscriptText(path);
        });
        connect(exportSrtBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getSaveFileName(this,
                "Export SRT", generateFilename(".srt"), "SRT Subtitle Files (*.srt)");
            if (!path.isEmpty()) m_mod->exportTranscriptSrt(path);
        });

        // State change handler
        connect(mod, &M1::TranscribeRecModule::stateChanged, this, [this](int state) {
            auto s = static_cast<M1::TranscribeRecModule::State>(state);
            switch (s) {
            case M1::TranscribeRecModule::State::Idle:
                m_stateLabel->setText("IDLE");
                m_recBtn->setText("Record");
                m_recBtn->setEnabled(true);
                m_pauseBtn->setEnabled(false);
                m_stopBtn->setEnabled(false);
                break;
            case M1::TranscribeRecModule::State::Recording:
                m_stateLabel->setText("RECORDING");
                m_recBtn->setEnabled(false);
                m_pauseBtn->setEnabled(true);
                m_stopBtn->setEnabled(true);
                break;
            case M1::TranscribeRecModule::State::Paused:
                m_stateLabel->setText("PAUSED");
                m_recBtn->setText("Resume");
                m_recBtn->setEnabled(true);
                m_pauseBtn->setEnabled(false);
                m_stopBtn->setEnabled(true);
                break;
            }
        });
    }

    void refresh() {
        // Update level meter
        float level = m_mod->inputLevel();
        m_levelBar->setValue(static_cast<int>(level * 100));

        // Update time display
        if (m_mod->recordingState() != M1::TranscribeRecModule::State::Idle) {
            m_timeLabel->setText(formatTime(m_mod->elapsedMs()));
        }
    }

private:
    QString generateFilename(const QString& ext) {
        QString title = m_mod->sermonTitle();
        if (title.isEmpty()) title = "sermon";
        title.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
        return QString("%1_%2%3")
            .arg(QDate::currentDate().toString("yyyy-MM-dd"), title, ext);
    }

    M1::TranscribeRecModule* m_mod;
    QLineEdit*    m_titleEdit   = nullptr;
    QLineEdit*    m_speakerEdit = nullptr;
    QProgressBar* m_levelBar    = nullptr;
    QLabel*       m_timeLabel   = nullptr;
    QLabel*       m_stateLabel  = nullptr;
    QPushButton*  m_recBtn      = nullptr;
    QPushButton*  m_pauseBtn    = nullptr;
    QPushButton*  m_stopBtn     = nullptr;
};

// ─── TranscriptPanel — editable transcript list ─────────────────────────────
class TranscriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit TranscriptPanel(M1::TranscribeRecModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("TranscribeRecTranscript");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);

        auto* header = new QLabel("Transcript");
        header->setObjectName("TranscribeRecTranscriptHeader");
        QFont hf;
        hf.setBold(true);
        header->setFont(hf);
        lay->addWidget(header);

        m_list = new QListWidget;
        m_list->setObjectName("TranscribeRecTranscriptList");
        lay->addWidget(m_list, 1);

        auto* btnRow = new QHBoxLayout;
        auto* addBtn = new QPushButton("+ Add Note");
        addBtn->setObjectName("TranscribeRecAddNote");
        auto* editBtn = new QPushButton("Edit");
        editBtn->setObjectName("TranscribeRecEditNote");
        auto* removeBtn = new QPushButton("Remove");
        removeBtn->setObjectName("TranscribeRecRemoveNote");
        btnRow->addWidget(addBtn);
        btnRow->addWidget(editBtn);
        btnRow->addWidget(removeBtn);
        lay->addLayout(btnRow);

        // Note about STT
        auto* sttLabel = new QLabel("Speech-to-text: not yet available (v2 feature)");
        sttLabel->setObjectName("TranscribeRecSttLabel");
        sttLabel->setStyleSheet("color: #888; font-style: italic; font-size: 12px;");
        lay->addWidget(sttLabel);

        connect(addBtn, &QPushButton::clicked, this, [this]() {
            bool ok = false;
            QString text = QInputDialog::getMultiLineText(this,
                "Add Transcript Note", "Enter text:", "", &ok);
            if (!ok || text.isEmpty()) return;
            m_mod->addTranscriptEntry(m_mod->elapsedMs(), text);
            rebuild();
        });
        connect(editBtn, &QPushButton::clicked, this, [this]() {
            int row = m_list->currentRow();
            if (row < 0) return;
            auto entries = m_mod->transcript();
            if (row >= entries.size()) return;
            bool ok = false;
            QString text = QInputDialog::getMultiLineText(this,
                "Edit Transcript Entry", "Edit text:", entries[row].text, &ok);
            if (!ok) return;
            m_mod->editTranscriptEntry(row, text);
            rebuild();
        });
        connect(removeBtn, &QPushButton::clicked, this, [this]() {
            int row = m_list->currentRow();
            if (row >= 0) {
                m_mod->removeTranscriptEntry(row);
                rebuild();
            }
        });
        connect(mod, &M1::TranscribeRecModule::transcriptUpdated, this, &TranscriptPanel::rebuild);
    }

    void rebuild() {
        m_list->clear();
        auto entries = m_mod->transcript();
        for (const auto& entry : entries) {
            QString label = QString("[%1] %2")
                .arg(formatTime(entry.timestampMs),
                     entry.text.left(80));
            if (entry.isManual) label += " (manual)";
            m_list->addItem(label);
        }
    }

private:
    M1::TranscribeRecModule* m_mod;
    QListWidget* m_list = nullptr;
};

// ─── TranscribeRecWidget — main composite widget ────────────────────────────
class TranscribeRecWidget : public QWidget {
    Q_OBJECT
public:
    explicit TranscribeRecWidget(M1::TranscribeRecModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("TranscribeRecWidget");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);

        auto* splitter = new QSplitter(Qt::Vertical);

        m_recordingPanel = new RecordingPanel(mod);
        splitter->addWidget(m_recordingPanel);

        m_transcriptPanel = new TranscriptPanel(mod);
        splitter->addWidget(m_transcriptPanel);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 2);
        lay->addWidget(splitter);

        // Refresh timer (100ms for level meter + time display)
        auto* refreshTimer = new QTimer(this);
        refreshTimer->setInterval(100);
        connect(refreshTimer, &QTimer::timeout, m_recordingPanel, &RecordingPanel::refresh);
        refreshTimer->start();
    }

private:
    M1::TranscribeRecModule* m_mod;
    RecordingPanel*  m_recordingPanel = nullptr;
    TranscriptPanel* m_transcriptPanel = nullptr;
};

} // anonymous namespace

#include "TranscribeRecModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
TranscribeRecModule::TranscribeRecModule(QObject* parent)
    : IModule(parent)
{
}

TranscribeRecModule::~TranscribeRecModule() {
    stopRecording();
}

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void TranscribeRecModule::initialize() {
    m_drainTimer = new QTimer(this);
    m_drainTimer->setInterval(50);  // 20Hz drain
    connect(m_drainTimer, &QTimer::timeout, this, &TranscribeRecModule::drainRingBuffer);
}

void TranscribeRecModule::shutdown() {
    stopRecording();
    if (m_drainTimer) m_drainTimer->stop();
}

QWidget* TranscribeRecModule::createWidget(QWidget* parent) {
    initialize();
    return new TranscribeRecWidget(this, parent);
}

// ─── RT audio processing ────────────────────────────────────────────────────
/// We capture the input audio (which comes from the PTT mic pipeline) and
/// write it to a lock-free ring buffer when recording is active.
void TranscribeRecModule::onAudioBlock(AudioBuffer& in, AudioBuffer& /*out*/) {
    if (!in.isValid) return;

    // Cache audio format on first call
    m_channels.store(in.channels, std::memory_order_relaxed);
    m_sampleRate.store(in.sampleRate, std::memory_order_relaxed);

    // Measure input level (RMS of first channel)
    float sum = 0.0f;
    for (int f = 0; f < in.frames; ++f) {
        float s = in.data[f * in.channels];
        sum += s * s;
    }
    float rms = sqrtf(sum / static_cast<float>(in.frames));
    m_inputLevel.store(rms, std::memory_order_relaxed);

    // Write to ring buffer if actively recording
    if (!m_captureActive.load(std::memory_order_relaxed)) return;

    int totalSamples = in.totalSamples();
    int wi = m_writeIdx.load(std::memory_order_relaxed);
    int ri = m_readIdx.load(std::memory_order_acquire);

    for (int i = 0; i < totalSamples; ++i) {
        int nextWi = (wi + 1) & (kRingSize - 1);
        if (nextWi == ri) break;  // Ring full — drop samples rather than block
        m_ring[wi] = in.data[i];
        wi = nextWi;
    }
    m_writeIdx.store(wi, std::memory_order_release);
}

void TranscribeRecModule::saveState(QSettings& s) {
    s.beginGroup("TranscribeRec");
    s.setValue("sermonTitle", m_sermonTitle);
    s.setValue("speaker", m_speaker);
    s.setValue("transcriptCount", m_transcript.size());
    for (int i = 0; i < m_transcript.size(); ++i) {
        s.beginGroup(QString("entry_%1").arg(i));
        s.setValue("timestamp", m_transcript[i].timestampMs);
        s.setValue("text", m_transcript[i].text);
        s.setValue("manual", m_transcript[i].isManual);
        s.endGroup();
    }
    s.endGroup();
}

void TranscribeRecModule::loadState(QSettings& s) {
    s.beginGroup("TranscribeRec");
    m_sermonTitle = s.value("sermonTitle").toString();
    m_speaker = s.value("speaker").toString();
    int count = s.value("transcriptCount", 0).toInt();
    m_transcript.clear();
    for (int i = 0; i < count; ++i) {
        s.beginGroup(QString("entry_%1").arg(i));
        TranscriptEntry entry;
        entry.timestampMs = s.value("timestamp", 0).toLongLong();
        entry.text = s.value("text").toString();
        entry.isManual = s.value("manual", false).toBool();
        m_transcript.append(entry);
        s.endGroup();
    }
    s.endGroup();
}

// ─── Recording control ─────────────────────────────────────────────────────
void TranscribeRecModule::startRecording() {
    if (recordingState() != State::Idle) return;

    openTempFile();
    if (!m_tempFile) {
        emit recordingError("Failed to create temp file for recording");
        return;
    }

    m_sampleCount = 0;
    m_writeIdx.store(0, std::memory_order_relaxed);
    m_readIdx.store(0, std::memory_order_relaxed);
    m_pausedMs = 0;
    m_elapsed.start();

    m_state.store(static_cast<int>(State::Recording), std::memory_order_relaxed);
    m_captureActive.store(true, std::memory_order_release);
    m_drainTimer->start();
    emit stateChanged(static_cast<int>(State::Recording));
}

void TranscribeRecModule::pauseRecording() {
    if (recordingState() != State::Recording) return;
    m_captureActive.store(false, std::memory_order_release);
    m_pausedMs += m_elapsed.elapsed();
    m_state.store(static_cast<int>(State::Paused), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(State::Paused));
}

void TranscribeRecModule::resumeRecording() {
    if (recordingState() != State::Paused) return;
    m_elapsed.start();
    m_captureActive.store(true, std::memory_order_release);
    m_state.store(static_cast<int>(State::Recording), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(State::Recording));
}

void TranscribeRecModule::stopRecording() {
    if (recordingState() == State::Idle) return;
    m_captureActive.store(false, std::memory_order_release);
    m_drainTimer->stop();

    // Drain remaining samples
    drainRingBuffer();
    closeTempFile();

    m_state.store(static_cast<int>(State::Idle), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(State::Idle));
}

TranscribeRecModule::State TranscribeRecModule::recordingState() const {
    return static_cast<State>(m_state.load(std::memory_order_relaxed));
}

qint64 TranscribeRecModule::elapsedMs() const {
    auto state = recordingState();
    if (state == State::Recording)
        return m_pausedMs + m_elapsed.elapsed();
    if (state == State::Paused)
        return m_pausedMs;
    return 0;
}

// ─── Transcript management ─────────────────────────────────────────────────
void TranscribeRecModule::addTranscriptEntry(qint64 timestampMs, const QString& text) {
    TranscriptEntry entry;
    entry.timestampMs = timestampMs;
    entry.text = text;
    entry.isManual = true;
    m_transcript.append(entry);
    emit transcriptUpdated();
}

void TranscribeRecModule::editTranscriptEntry(int index, const QString& text) {
    if (index >= 0 && index < m_transcript.size()) {
        m_transcript[index].text = text;
        emit transcriptUpdated();
    }
}

void TranscribeRecModule::removeTranscriptEntry(int index) {
    if (index >= 0 && index < m_transcript.size()) {
        m_transcript.removeAt(index);
        emit transcriptUpdated();
    }
}

void TranscribeRecModule::clearTranscript() {
    m_transcript.clear();
    emit transcriptUpdated();
}

// ─── Export ─────────────────────────────────────────────────────────────────
bool TranscribeRecModule::exportWav(const QString& filePath, QString& errorOut) {
    if (!m_tempFile || m_sampleCount == 0) {
        errorOut = "No recording data available";
        return false;
    }

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        errorOut = "Cannot open file for writing: " + filePath;
        return false;
    }

    int ch = m_channels.load(std::memory_order_relaxed);
    double sr = m_sampleRate.load(std::memory_order_relaxed);
    writeWavHeader(outFile, ch, sr, m_sampleCount);

    // Copy PCM data from temp file
    m_tempFile->seek(0);
    while (!m_tempFile->atEnd()) {
        QByteArray chunk = m_tempFile->read(65536);
        outFile.write(chunk);
    }
    outFile.close();

    emit exportFinished(true, filePath);
    return true;
}

bool TranscribeRecModule::exportTranscriptText(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream ts(&file);
    ts << "Sermon: " << m_sermonTitle << "\n";
    ts << "Speaker: " << m_speaker << "\n";
    ts << "Date: " << QDate::currentDate().toString("yyyy-MM-dd") << "\n";
    ts << "---\n\n";

    for (const auto& entry : m_transcript) {
        ts << "[" << formatTime(entry.timestampMs) << "] " << entry.text << "\n\n";
    }

    file.close();
    emit exportFinished(true, filePath);
    return true;
}

bool TranscribeRecModule::exportTranscriptSrt(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream ts(&file);
    for (int i = 0; i < m_transcript.size(); ++i) {
        qint64 startMs = m_transcript[i].timestampMs;
        qint64 endMs = (i + 1 < m_transcript.size())
            ? m_transcript[i + 1].timestampMs
            : startMs + 5000;  // Default 5s duration for last entry

        ts << (i + 1) << "\n";
        ts << srtTimestamp(startMs) << " --> " << srtTimestamp(endMs) << "\n";
        ts << m_transcript[i].text << "\n\n";
    }

    file.close();
    emit exportFinished(true, filePath);
    return true;
}

// ─── Ring buffer drain ──────────────────────────────────────────────────────
void TranscribeRecModule::drainRingBuffer() {
    if (!m_tempFile || !m_tempFile->isOpen()) return;

    int ri = m_readIdx.load(std::memory_order_relaxed);
    int wi = m_writeIdx.load(std::memory_order_acquire);

    while (ri != wi) {
        int available = (wi - ri + kRingSize) & (kRingSize - 1);
        int toRead = qMin(available, kDrainBuf);

        for (int i = 0; i < toRead; ++i) {
            m_drainScratch[i] = m_ring[(ri + i) & (kRingSize - 1)];
        }

        m_tempFile->write(reinterpret_cast<const char*>(m_drainScratch),
                          toRead * sizeof(float));
        m_sampleCount += toRead;

        ri = (ri + toRead) & (kRingSize - 1);
        m_readIdx.store(ri, std::memory_order_release);
        wi = m_writeIdx.load(std::memory_order_acquire);
    }
}

// ─── Temp file management ───────────────────────────────────────────────────
void TranscribeRecModule::openTempFile() {
    closeTempFile();
    m_tempFile = new QTemporaryFile(this);
    m_tempFile->setAutoRemove(true);
    if (!m_tempFile->open()) {
        delete m_tempFile;
        m_tempFile = nullptr;
    }
    m_sampleCount = 0;
}

void TranscribeRecModule::closeTempFile() {
    // We keep the temp file alive for export — it's deleted on destruction
}

bool TranscribeRecModule::writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount) {
    qint64 dataSize = sampleCount * sizeof(float);
    qint64 fileSize = 36 + dataSize;

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    file.write("RIFF", 4);
    ds << static_cast<quint32>(fileSize);
    file.write("WAVE", 4);

    file.write("fmt ", 4);
    ds << quint32(16);
    ds << quint16(3);  // IEEE float
    ds << quint16(channels);
    ds << quint32(static_cast<quint32>(sampleRate));
    ds << quint32(static_cast<quint32>(sampleRate * channels * sizeof(float)));
    ds << quint16(channels * sizeof(float));
    ds << quint16(32);

    file.write("data", 4);
    ds << quint32(dataSize);

    return true;
}

} // namespace M1
