#pragma once
/// @file   PodPTTModule.h
/// @path   Modules/PodPTTModule/PodPTTModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodPTT — Enhanced Podcast Push-to-Talk Module
/// @purpose Extended PTT for podcast production with cough button,
///          talkback, VAD indicator, hold/toggle/always-on modes,
///          and full DSP chain (gate, de-esser, compressor, HPF, limiter).
/// @reason  Podcast hosts need more control than basic PTT — cough mute,
///          talkback to guests, and multiple activation modes.
/// @changelog
///   2026-03-09  Initial implementation — multi-mode PTT, cough, talkback, DSP

#include "IModule.h"
#include <atomic>

class QTimer;

namespace M1 {

class PodPTTModule : public IModule {
    Q_OBJECT

public:
    enum class ActivationMode { HoldToTalk, Toggle, AlwaysOn };
    enum class State { Off, Armed, Live, Cough };

    explicit PodPTTModule(QObject* parent = nullptr);
    ~PodPTTModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.ptt"; }
    QString displayName() const override { return "Podcast PTT"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {300, 350}; }
    QSize minimumModuleSize() const override { return {240, 280}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // State control
    void setState(State s);
    State state() const { return static_cast<State>(m_state.load(std::memory_order_relaxed)); }
    void setActivationMode(ActivationMode mode);
    ActivationMode activationMode() const { return m_actMode; }

    // Cough button (fade-mute)
    void coughStart();
    void coughEnd();
    bool isCoughing() const { return state() == State::Cough; }

    // Talkback (headphones only, not recorded)
    void setTalkback(bool on) { m_talkback.store(on, std::memory_order_relaxed); }
    bool talkback() const { return m_talkback.load(std::memory_order_relaxed); }

    // Input device
    void setInputDeviceId(const QString& id) { m_inputDevice = id; }
    QString inputDeviceId() const { return m_inputDevice; }

    // DSP parameters
    void setGateThreshold(float v) { m_gateThresh.store(v, std::memory_order_relaxed); }
    void setDeEssAmount(float v)   { m_deEss.store(v, std::memory_order_relaxed); }
    void setCompThreshold(float v) { m_compThresh.store(v, std::memory_order_relaxed); }
    void setCompRatio(float v)     { m_compRatio.store(v, std::memory_order_relaxed); }
    void setHpfFreq(float hz)      { m_hpfFreq.store(hz, std::memory_order_relaxed); }
    void setGain(float v)          { m_gain.store(v, std::memory_order_relaxed); }

    float gateThreshold() const { return m_gateThresh.load(std::memory_order_relaxed); }
    float deEssAmount()   const { return m_deEss.load(std::memory_order_relaxed); }
    float compThreshold() const { return m_compThresh.load(std::memory_order_relaxed); }
    float compRatio()     const { return m_compRatio.load(std::memory_order_relaxed); }
    float hpfFreq()       const { return m_hpfFreq.load(std::memory_order_relaxed); }
    float gain()          const { return m_gain.load(std::memory_order_relaxed); }

    // Level meter
    float inputLevel() const { return m_level.load(std::memory_order_relaxed); }
    bool  voiceActive() const { return m_vad.load(std::memory_order_relaxed); }

    // Participant name
    void setParticipantName(const QString& name) { m_participantName = name; }
    QString participantName() const { return m_participantName; }

signals:
    void stateChanged(int newState);
    void voiceActivityChanged(bool active);

private:
    std::atomic<int>   m_state{static_cast<int>(State::Off)};
    ActivationMode     m_actMode = ActivationMode::HoldToTalk;
    State              m_prevState = State::Off; // for cough restore
    std::atomic<bool>  m_talkback{false};
    QString            m_inputDevice;
    QString            m_participantName;

    // DSP
    std::atomic<float> m_gateThresh{0.02f};
    std::atomic<float> m_deEss{0.0f};
    std::atomic<float> m_compThresh{0.7f};
    std::atomic<float> m_compRatio{3.0f};
    std::atomic<float> m_hpfFreq{80.0f};
    std::atomic<float> m_gain{1.0f};

    // Metering
    std::atomic<float> m_level{0.0f};
    std::atomic<bool>  m_vad{false};

    // Cough fade envelope
    std::atomic<float> m_coughGain{1.0f};

    QTimer* m_meterTimer = nullptr;
};

} // namespace M1
