#pragma once
/// @file   PodMixerModule.h
/// @path   Modules/PodMixerModule/PodMixerModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodMixer — Podcast Desktop Mixer Module
/// @purpose Six-channel desktop mixer with per-channel gain, 3-band EQ,
///          aux sends, pan, solo/mute, LED meters, and master bus.
///          Routes to PodRecorder and live encoder.
/// @reason  Podcast production requires mixing multiple audio sources
///          (host mic, guest mics, soundboard, music beds) with
///          independent processing and monitoring.
/// @changelog
///   2026-03-09  Initial implementation — 6-channel mixer, master bus, aux sends

#include "IModule.h"
#include <atomic>
#include <array>

class QTimer;

namespace M1 {

static constexpr int kPodMixChannels = 6;
static constexpr int kPodAuxBuses    = 2;

// ─── PodMixChannel — per-channel strip state ────────────────────────────────
struct PodMixChannel {
    QString name;

    std::atomic<float> gainTrim{1.0f};    ///< Input gain (pre-fader)
    std::atomic<float> volume{1.0f};      ///< Fader 0.0–1.0
    std::atomic<float> pan{0.0f};         ///< -1.0 (L) to +1.0 (R)
    std::atomic<bool>  muted{false};
    std::atomic<bool>  solo{false};

    // 3-band EQ (dB: -12 to +12)
    std::atomic<float> eqHigh{0.0f};
    std::atomic<float> eqMid{0.0f};
    std::atomic<float> eqLow{0.0f};

    // Aux sends
    std::array<std::atomic<float>, kPodAuxBuses> auxSend;

    // Level meters (RT → UI)
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};

    // Record arm
    std::atomic<bool> recordArm{true};

    PodMixChannel() {
        for (auto& s : auxSend) s.store(0.0f, std::memory_order_relaxed);
    }
};

// ─── PodMixerModule ─────────────────────────────────────────────────────────
class PodMixerModule : public IModule {
    Q_OBJECT

public:
    explicit PodMixerModule(QObject* parent = nullptr);
    ~PodMixerModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.mixer"; }
    QString displayName() const override { return "Podcast Mixer"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {700, 400}; }
    QSize minimumModuleSize() const override { return {500, 300}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Channel access
    int channelCount() const { return kPodMixChannels; }
    PodMixChannel& channel(int i) { return m_channels[i]; }
    const PodMixChannel& channel(int i) const { return m_channels[i]; }

    // Master bus
    void  setMasterVolume(float v) { m_masterVol.store(v, std::memory_order_relaxed); }
    float masterVolume() const { return m_masterVol.load(std::memory_order_relaxed); }
    float masterPeakL() const { return m_masterPeakL.load(std::memory_order_relaxed); }
    float masterPeakR() const { return m_masterPeakR.load(std::memory_order_relaxed); }

    // Headphone / monitor
    void  setHeadphoneVolume(float v) { m_hpVol.store(v, std::memory_order_relaxed); }
    float headphoneVolume() const { return m_hpVol.load(std::memory_order_relaxed); }

    // Aux bus names
    void setAuxName(int bus, const QString& name);
    QString auxName(int bus) const;

signals:
    void channelLevelsUpdated();

private:
    std::array<PodMixChannel, kPodMixChannels> m_channels;
    std::atomic<float> m_masterVol{1.0f};
    std::atomic<float> m_masterPeakL{0.0f};
    std::atomic<float> m_masterPeakR{0.0f};
    std::atomic<float> m_hpVol{0.8f};
    std::array<QString, kPodAuxBuses> m_auxNames;
    QTimer* m_meterTimer = nullptr;
};

} // namespace M1
