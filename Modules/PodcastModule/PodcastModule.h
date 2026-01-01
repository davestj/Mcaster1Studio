#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QObject>
#include <QList>
#include <QString>
#include <QElapsedTimer>
#include <atomic>
#include <array>
#include <cstdint>

class QTemporaryFile;
class PodcastWidget;

namespace M1 {

/// PodcastChapter — a timed chapter marker within a podcast recording session.
struct PodcastChapter {
    qint64  positionMs = 0;     ///< Elapsed time in milliseconds at chapter start
    QString title;              ///< Chapter display title
};

/// PodcastModule — Phase 8 multi-track podcast session recorder.
///
/// Captures up to kMaxTracks simultaneous tracks from the AudioBuffer input.
/// Raw float32 PCM is captured in onAudioBlock() (RT thread) via lock-free ring
/// buffers. A background QThread drains ring buffers to QTemporaryFile storage.
///
/// Playback monitoring is provided by QAudioSink (Qt Multimedia).
///
/// Export:
///   WAV  — writes PCM header + raw float data (implemented)
///   MP3  — stub (qWarning + TODO)
///   Opus — stub (qWarning + TODO)
///   FLAC — stub (qWarning + TODO)
///
/// State machine:
///   Idle → Armed → Recording ↔ Paused → Idle (Stop)
///
/// RT constraints:
///   onAudioBlock() writes only to atomic<bool>/atomic<float> and ring buffer.
///   NO Qt API calls, NO allocation, NO mutex wait in onAudioBlock().
class PodcastModule : public IModule {
    Q_OBJECT

public:
    static constexpr int kMaxTracks = 4;

    /// Ring buffer size in float samples per track (must be power-of-two).
    /// At 48kHz stereo, 65536 floats = ~0.68 seconds per track — enough
    /// headroom for the Qt drain thread to keep up.
    static constexpr int kRingSize  = 65536;

    enum class State { Idle, Armed, Recording, Paused };

    explicit PodcastModule(QObject* parent = nullptr);
    ~PodcastModule() override;

    // ── IModule identity ─────────────────────────────────────────────────
    QString  moduleId()          const override { return "com.mcaster1.podcast"; }
    QString  displayName()       const override { return "Podcast"; }
    QString  version()           const override { return "1.0.0"; }
    QSize    preferredSize()     const override { return {600, 400}; }
    QSize    minimumModuleSize() const override { return {480, 300}; }

    // ── IModule lifecycle ────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;

    // ── IModule UI ───────────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;

    // ── IModule audio (RT thread — no Qt, no alloc, no mutex wait) ──────
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    // ── IModule state persistence ────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Session control (Qt thread) ───────────────────────────────────────
    void arm();
    void record();
    void pause();
    void stop();

    State         sessionState() const;
    qint64        elapsedMs()    const;

    // ── Chapter management (Qt thread) ────────────────────────────────────
    void addChapter(const QString& title);
    void removeChapter(int index);
    const QList<PodcastChapter>& chapters() const { return m_chapters; }

    // ── Export (Qt thread) ────────────────────────────────────────────────
    bool exportWav(const QString& filePath, QString& errorOut);
    void exportMp3(const QString& filePath);    // stub
    void exportOpus(const QString& filePath);   // stub
    void exportFlac(const QString& filePath);   // stub

    // ── Track level meters (written in RT, polled at 50ms from UI) ───────
    float trackLevel(int track) const {
        if (track < 0 || track >= kMaxTracks) return 0.0f;
        return m_trackLevel[track].load(std::memory_order_relaxed);
    }

    /// Track name (e.g. "Track 1") — editable from UI
    QString trackName(int track) const;
    void    setTrackName(int track, const QString& name);

signals:
    void stateChanged(M1::PodcastModule::State newState);
    void recordingError(const QString& msg);
    void exportFinished(bool success, const QString& path);

private slots:
    void drainRingBuffers();    ///< Called by QTimer to flush ring → temp files

private:
    // ── Lock-free ring buffer ─────────────────────────────────────────────
    /// Simple single-producer (RT) / single-consumer (Qt drain) ring buffer.
    /// Uses std::atomic head/tail indices. Only one track per ring buffer.
    struct RingBuffer {
        std::array<float, kRingSize> data{};
        std::atomic<int>  writeIdx{0};
        std::atomic<int>  readIdx{0};

        /// Write n samples from src — returns samples actually written.
        /// Called from RT thread. If buffer is full, excess samples are dropped.
        int write(const float* src, int n) noexcept;

        /// Read up to n samples into dst — returns samples actually read.
        /// Called from Qt drain thread.
        int read(float* dst, int n) noexcept;

        /// Available samples to read
        int available() const noexcept;
    };

    // ── Members ───────────────────────────────────────────────────────────
    std::atomic<int>          m_state{static_cast<int>(State::Idle)};
    std::atomic<bool>         m_recording{false};   ///< RT fast-path flag

    QList<PodcastChapter>     m_chapters;
    QList<QString>            m_trackNames;

    // Per-track ring buffers (written in RT, drained in Qt thread)
    std::array<RingBuffer, kMaxTracks>    m_rings;
    // Per-track level meters
    std::array<std::atomic<float>, kMaxTracks> m_trackLevel;

    // Per-track temp PCM storage (Qt thread only)
    std::array<QTemporaryFile*, kMaxTracks> m_tempFiles{};
    std::array<qint64, kMaxTracks>          m_sampleCount{};  ///< Float samples written

    // Elapsed timing
    QElapsedTimer m_elapsed;
    qint64        m_pausedMs = 0;

    // Drain timer (Qt thread)
    QTimer*       m_drainTimer = nullptr;

    // Audio format cache (set on first onAudioBlock call)
    std::atomic<int>    m_channels{2};
    std::atomic<double> m_sampleRate{48000.0};

    // Drain scratch buffer (Qt thread only — no allocation in RT)
    static constexpr int kDrainBuf = 4096;
    float m_drainScratch[kDrainBuf]{};

    PodcastWidget* m_widget = nullptr;

    // ── Helpers ───────────────────────────────────────────────────────────
    void openTempFiles();
    void closeTempFiles();
    bool writePcmHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount);
};

} // namespace M1

Q_DECLARE_METATYPE(M1::PodcastModule::State)

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_podcast_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_podcast_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_podcast_destroy(M1::IModule*);
}
