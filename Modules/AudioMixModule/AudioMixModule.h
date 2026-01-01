#pragma once
/// @file   AudioMixModule.h
/// @path   Modules/AudioMixModule/AudioMixModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-AudioMix — Live Service Audio Mixer Module
/// @purpose Provides a virtual mixing console for church services with
///          per-channel volume, pan, mute, solo controls, per-channel DSP
///          processing, master bus, aux sends for monitor mixes, and
///          main mix recording to file.
/// @reason  Church services require live audio mixing of pastor mic, worship
///          vocals, instruments, and media playback into a cohesive output.
/// @changelog
///   2026-03-09  Initial implementation — channel strips, master bus, recording

#include "IModule.h"
#include <QList>
#include <atomic>
#include <array>
#include <cstdint>

class QTimer;
class QTemporaryFile;

namespace M1 {

/// Maximum number of input channels we support
static constexpr int kMaxMixChannels = 8;

/// Maximum number of aux send buses
static constexpr int kMaxAuxBuses = 4;

// ─── MixChannel — per-channel strip state ───────────────────────────────────
/// We use atomic floats for all parameters that the RT audio thread reads
/// and the Qt UI thread writes. This eliminates the need for mutexes.
struct MixChannel {
    QString name;                                     ///< "Pastor Mic", "Vocals", etc.

    // RT-safe parameters (atomic for lock-free UI↔RT)
    std::atomic<float> volume{1.0f};                  ///< 0.0–1.0 fader
    std::atomic<float> pan{0.0f};                     ///< -1.0 (left) to +1.0 (right)
    std::atomic<bool>  muted{false};
    std::atomic<bool>  solo{false};
    std::atomic<float> gain{1.0f};                    ///< Input gain (pre-fader)

    // Per-channel EQ (3-band: low, mid, high shelf)
    std::atomic<float> eqLow{0.0f};                   ///< dB gain (-12 to +12)
    std::atomic<float> eqMid{0.0f};
    std::atomic<float> eqHigh{0.0f};

    // Compressor
    std::atomic<float> compThreshold{0.8f};
    std::atomic<float> compRatio{2.0f};
    std::atomic<bool>  compEnabled{false};

    // Aux sends (0.0–1.0 level per bus)
    std::array<std::atomic<float>, kMaxAuxBuses> auxSend;

    // Level meter (written in RT, read by UI timer)
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};

    MixChannel() {
        for (auto& s : auxSend) s.store(0.0f, std::memory_order_relaxed);
    }
};

// ─── AudioMixModule ─────────────────────────────────────────────────────────
class AudioMixModule : public IModule {
    Q_OBJECT

public:
    explicit AudioMixModule(QObject* parent = nullptr);
    ~AudioMixModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.audiomix"; }
    QString displayName() const override { return "Audio Mixer"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {700, 400}; }
    QSize minimumModuleSize() const override { return {500, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Channel management ──────────────────────────────────────────────────
    int channelCount() const { return m_channelCount; }
    void setChannelCount(int count);
    MixChannel& channel(int index) { return m_channels[index]; }
    const MixChannel& channel(int index) const { return m_channels[index]; }

    // ── Master bus ──────────────────────────────────────────────────────────
    void setMasterVolume(float vol);
    float masterVolume() const { return m_masterVolume.load(std::memory_order_relaxed); }
    float masterPeakL() const { return m_masterPeakL.load(std::memory_order_relaxed); }
    float masterPeakR() const { return m_masterPeakR.load(std::memory_order_relaxed); }

    // ── Recording ───────────────────────────────────────────────────────────
    void startRecording(const QString& outputPath);
    void stopRecording();
    bool isRecording() const { return m_recording.load(std::memory_order_relaxed); }
    qint64 recordedMs() const;

    // ── Aux bus names ───────────────────────────────────────────────────────
    void setAuxBusName(int bus, const QString& name);
    QString auxBusName(int bus) const;

signals:
    void channelCountChanged(int count);
    void recordingStateChanged(bool recording);
    void recordingError(const QString& msg);

private:
    // Channel strips
    std::array<MixChannel, kMaxMixChannels> m_channels;
    int m_channelCount = 4;   ///< Active channel count (default: Pastor, Vocals, Band, Media)

    // Master bus
    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};

    // Recording state
    std::atomic<bool>  m_recording{false};
    QTemporaryFile*    m_recTempFile = nullptr;
    qint64             m_recSampleCount = 0;
    std::atomic<int>   m_recChannels{2};
    std::atomic<double> m_recSampleRate{48000.0};

    // Compressor envelope state (per channel, RT only)
    std::array<float, kMaxMixChannels> m_compEnv{};

    // EQ one-pole filter state (per-channel, 3-band crossover)
    struct EqFilterState {
        float lpL = 0, lpR = 0;  // lowpass state for low band extraction
        float hpL = 0, hpR = 0;  // lowpass state for high band extraction
    };
    std::array<EqFilterState, kMaxMixChannels> m_eqState{};

    // Aux bus names
    std::array<QString, kMaxAuxBuses> m_auxNames;

    // Ring buffer for recording (lock-free SPSC)
    static constexpr int kRecRingSize = 65536;
    std::array<float, kRecRingSize> m_recRing{};
    std::atomic<int> m_recWriteIdx{0};
    std::atomic<int> m_recReadIdx{0};

    // Drain timer for recording
    QTimer* m_recDrainTimer = nullptr;

    void drainRecordingBuffer();
    bool writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount);
};

} // namespace M1
