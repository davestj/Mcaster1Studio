#pragma once
/// @file   PodRecorderModule.h
/// @path   Modules/PodRecorderModule/PodRecorderModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodRecorder — Multi-Track Podcast Recording Module
/// @purpose Multi-track recording engine: captures each mixer channel to
///          isolated WAV tracks plus stereo mix. Supports markers, takes,
///          punch-in/out, and auto-naming.
/// @reason  Podcast production requires per-participant isolated tracks
///          for independent post-processing and noise removal.
/// @changelog
///   2026-03-09  Initial implementation — multi-track WAV, markers, takes

#include "IModule.h"
#include <QList>
#include <QElapsedTimer>
#include <atomic>
#include <array>

class QTimer;
class QTemporaryFile;

namespace M1 {

static constexpr int kMaxRecTracks = 8; // 6 channels + stereo mix

struct RecordMarker {
    qint64  timestampMs = 0;
    QString label;
    QColor  color;
};

class PodRecorderModule : public IModule {
    Q_OBJECT

public:
    enum class State { Idle, Recording, Paused };

    explicit PodRecorderModule(QObject* parent = nullptr);
    ~PodRecorderModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.recorder"; }
    QString displayName() const override { return "Podcast Recorder"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {550, 350}; }
    QSize minimumModuleSize() const override { return {400, 260}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Recording control
    void startRecording();
    void pauseRecording();
    void resumeRecording();
    void stopRecording();
    State recordingState() const { return static_cast<State>(m_state.load(std::memory_order_relaxed)); }
    qint64 elapsedMs() const;

    // Show / episode metadata
    void setShowName(const QString& name) { m_showName = name; }
    QString showName() const { return m_showName; }
    void setEpisodeNumber(int num) { m_episodeNum = num; }
    int episodeNumber() const { return m_episodeNum; }

    // Markers
    void addMarker(const QString& label = {});
    void removeMarker(int index);
    QList<RecordMarker> markers() const { return m_markers; }

    // Takes
    int currentTake() const { return m_takeNumber; }

    // Track arm
    void setTrackArm(int track, bool arm);
    bool trackArm(int track) const;
    int trackCount() const { return m_trackCount; }
    void setTrackCount(int count);

    // Export
    bool exportAll(const QString& outputDir, QString& errorOut);
    QString autoFileName() const;

    // Disk space estimate
    qint64 estimatedFileSizeBytes() const;

    // Audio format
    int sampleRateHz() const { return static_cast<int>(m_sampleRate.load(std::memory_order_relaxed)); }
    int bitDepth() const { return m_bitDepth; }
    void setBitDepth(int bits) { m_bitDepth = bits; } // 16 or 24

signals:
    void stateChanged(int newState);
    void markerAdded(int index);
    void recordingError(const QString& msg);
    void diskSpaceWarning(qint64 remainingBytes);

private slots:
    void drainBuffers();

private:
    std::atomic<int>    m_state{static_cast<int>(State::Idle)};
    std::atomic<bool>   m_captureActive{false};
    std::atomic<double> m_sampleRate{48000.0};
    std::atomic<int>    m_channels{2};

    QString m_showName;
    int     m_episodeNum = 1;
    int     m_takeNumber = 1;
    int     m_bitDepth   = 24;
    int     m_trackCount = 2; // stereo mix by default

    QElapsedTimer m_elapsed;
    qint64 m_pausedMs = 0;

    // Markers
    QList<RecordMarker> m_markers;

    // Track arm states
    std::array<std::atomic<bool>, kMaxRecTracks> m_trackArms;

    // Ring buffer (SPSC)
    static constexpr int kRingSize = 65536;
    std::array<float, kRingSize> m_ring{};
    std::atomic<int> m_writeIdx{0};
    std::atomic<int> m_readIdx{0};

    // Temp file + drain
    QTemporaryFile* m_tempFile = nullptr;
    qint64 m_sampleCount = 0;
    QTimer* m_drainTimer = nullptr;
    static constexpr int kDrainBuf = 4096;
    float m_drainScratch[kDrainBuf]{};

    void openTempFile();
    void closeTempFile();
    bool writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount);
};

} // namespace M1
