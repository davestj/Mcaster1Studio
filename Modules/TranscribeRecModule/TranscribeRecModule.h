#pragma once
/// @file   TranscribeRecModule.h
/// @path   Modules/TranscribeRecModule/TranscribeRecModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TranscribeRec — Sermon Recording and Transcription Module
/// @purpose Captures the pastor's sermon audio from the PTT mic pipeline,
///          records to WAV for archival, and provides a transcript editing
///          interface. Speech-to-text transcription is stubbed for v1 —
///          integration with Whisper or cloud STT can be added later.
/// @reason  Churches need sermon recordings for web publishing, podcast
///          distribution, and accessibility. Transcript editing enables
///          sermon notes and subtitle generation.
/// @changelog
///   2026-03-09  Initial implementation — recorder, transcript editor, WAV export

#include "IModule.h"
#include "IThreadPoolAware.h"
#include <QList>
#include <QElapsedTimer>
#include <atomic>
#include <array>

class QTimer;
class QTemporaryFile;

namespace M1 {

class PTTModule;

// ─── TranscriptEntry — a timestamped text segment ──────────────────────────
struct TranscriptEntry {
    qint64  timestampMs = 0;    ///< Position in the recording
    QString text;                ///< Transcribed text
    bool    isManual = false;    ///< true if manually entered, false if STT
};

// ─── TranscribeRecModule ────────────────────────────────────────────────────
class TranscribeRecModule : public IModule, public IThreadPoolAware {
    Q_OBJECT

public:
    enum class State { Idle, Recording, Paused };

    explicit TranscribeRecModule(QObject* parent = nullptr);
    ~TranscribeRecModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.transcriberec"; }
    QString displayName() const override { return "Transcription Recorder"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {550, 400}; }
    QSize minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── PTT module binding ──────────────────────────────────────────────────
    void setPTTModule(PTTModule* ptt) { m_ptt = ptt; }

    // ── Recording control ───────────────────────────────────────────────────
    void startRecording();
    void pauseRecording();
    void resumeRecording();
    void stopRecording();
    State recordingState() const;
    qint64 elapsedMs() const;

    // ── Sermon metadata ─────────────────────────────────────────────────────
    void setSermonTitle(const QString& title) { m_sermonTitle = title; }
    QString sermonTitle() const { return m_sermonTitle; }
    void setSpeaker(const QString& name) { m_speaker = name; }
    QString speaker() const { return m_speaker; }

    // ── Transcript management ───────────────────────────────────────────────
    void addTranscriptEntry(qint64 timestampMs, const QString& text);
    void editTranscriptEntry(int index, const QString& text);
    void removeTranscriptEntry(int index);
    QList<TranscriptEntry> transcript() const { return m_transcript; }
    void clearTranscript();

    // ── Export ───────────────────────────────────────────────────────────────
    bool exportWav(const QString& filePath, QString& errorOut);
    bool exportTranscriptText(const QString& filePath);
    bool exportTranscriptSrt(const QString& filePath);

    // ── Level meter ─────────────────────────────────────────────────────────
    float inputLevel() const { return m_inputLevel.load(std::memory_order_relaxed); }

signals:
    void stateChanged(int newState);
    void transcriptUpdated();
    void exportFinished(bool success, const QString& path);
    void recordingError(const QString& msg);

private slots:
    void drainRingBuffer();

private:
    PTTModule* m_ptt = nullptr;

    // State
    std::atomic<int>  m_state{static_cast<int>(State::Idle)};
    std::atomic<bool> m_captureActive{false};

    // Sermon metadata
    QString m_sermonTitle;
    QString m_speaker;

    // Transcript
    QList<TranscriptEntry> m_transcript;

    // Recording ring buffer (lock-free SPSC)
    static constexpr int kRingSize = 65536;
    std::array<float, kRingSize> m_ring{};
    std::atomic<int> m_writeIdx{0};
    std::atomic<int> m_readIdx{0};

    // Temp file storage
    QTemporaryFile* m_tempFile = nullptr;
    qint64 m_sampleCount = 0;

    // Timing
    QElapsedTimer m_elapsed;
    qint64 m_pausedMs = 0;

    // Audio format
    std::atomic<int>    m_channels{2};
    std::atomic<double> m_sampleRate{48000.0};

    // Level meter
    std::atomic<float> m_inputLevel{0.0f};

    // Drain timer
    QTimer* m_drainTimer = nullptr;

    // Helpers
    void openTempFile();
    void closeTempFile();
    bool writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount);

    // Drain scratch buffer
    static constexpr int kDrainBuf = 4096;
    float m_drainScratch[kDrainBuf]{};

    // IThreadPoolAware
    void setSurfaceThreadPool(SurfaceThreadPool* pool) override { m_pool = pool; }
    SurfaceThreadPool* surfaceThreadPool() const override { return m_pool; }
    SurfaceThreadPool* m_pool = nullptr;
};

} // namespace M1
