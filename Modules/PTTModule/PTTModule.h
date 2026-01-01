#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QObject>
#include <atomic>

class PTTWidget;

namespace M1 {

/// PTTModule — Phase 8 Push-To-Talk microphone module.
///
/// Provides a virtual microphone preamp chain for live broadcast and podcast
/// recording on Podcast and Church surfaces. The DSP chain processes mic input
/// through noise gate → de-esser → compressor → EQ (shelf) in the RT thread.
///
/// State machine: Off → Armed → Live → Armed/Off
///   Off:    module is idle; no audio passed
///   Armed:  mic is hot-standby; DSP runs but output muted
///   Live:   PTT button held; mic audio mixed into output
///
/// All DSP parameters are std::atomic<float> so the Qt UI thread can adjust
/// them without locking the RT audio thread.
class PTTModule : public IModule {
    Q_OBJECT

public:
    enum class State { Off, Armed, Live };

    explicit PTTModule(QObject* parent = nullptr);
    ~PTTModule() override;

    // ── IModule identity ─────────────────────────────────────────────────
    QString  moduleId()          const override { return "com.mcaster1.ptt"; }
    QString  displayName()       const override { return "PTT Mic"; }
    QString  version()           const override { return "1.0.0"; }
    QSize    preferredSize()     const override { return {300, 280}; }
    QSize    minimumModuleSize() const override { return {240, 220}; }

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

    // ── PTT control (called from UI thread) ──────────────────────────────
    void setState(State s);
    State state() const;

    // ── Input device selection ────────────────────────────────────────────
    void    setInputDeviceId(const QString& id) { m_inputDeviceId = id; }
    QString inputDeviceId() const               { return m_inputDeviceId; }

    // ── DSP parameter access (UI thread writes, RT thread reads) ─────────
    void setGateThreshold(float linear) { m_gateThreshold.store(linear, std::memory_order_relaxed); }
    void setDeEssAmount(float linear)   { m_deEssAmount.store(linear,   std::memory_order_relaxed); }
    void setCompThreshold(float linear) { m_compThreshold.store(linear, std::memory_order_relaxed); }
    void setCompRatio(float ratio)      { m_compRatio.store(ratio,      std::memory_order_relaxed); }
    void setCompAttack(float ms)        { m_compAttack.store(ms,        std::memory_order_relaxed); }
    void setCompRelease(float ms)       { m_compRelease.store(ms,       std::memory_order_relaxed); }
    void setGain(float linear)          { m_gain.store(linear,          std::memory_order_relaxed); }

    float gateThreshold() const { return m_gateThreshold.load(std::memory_order_relaxed); }
    float deEssAmount()   const { return m_deEssAmount.load(std::memory_order_relaxed); }
    float compThreshold() const { return m_compThreshold.load(std::memory_order_relaxed); }
    float compRatio()     const { return m_compRatio.load(std::memory_order_relaxed); }
    float gain()          const { return m_gain.load(std::memory_order_relaxed); }

    /// Input level (polled by UI timer at 50ms) — set in RT thread
    float inputLevel() const { return m_inputLevel.load(std::memory_order_relaxed); }

signals:
    void stateChanged(M1::PTTModule::State newState);

private:
    // ── DSP helpers (RT thread only) ─────────────────────────────────────
    float computeRms(const float* data, int frames, int channels, int ch) const noexcept;
    float computeBandEnergy(const float* data, int frames, int channels,
                            int ch, double sampleRate,
                            float freqLow, float freqHigh) const noexcept;
    void  applyLowShelfBoost(float* data, int frames, int channels,
                             int ch, double sampleRate, float fc, float gainDb) noexcept;
    void  applyHighShelfCut(float* data, int frames, int channels,
                            int ch, double sampleRate, float fc, float gainDb) noexcept;

    // ── State ─────────────────────────────────────────────────────────────
    std::atomic<int>   m_state{static_cast<int>(State::Off)};

    // ── DSP parameters (atomics — written from Qt thread, read from RT) ──
    std::atomic<float> m_gateThreshold{0.01f};   ///< RMS below which gate closes
    std::atomic<float> m_deEssAmount{0.5f};      ///< De-esser attenuation (0..1)
    std::atomic<float> m_compThreshold{0.7f};    ///< Peak compressor threshold
    std::atomic<float> m_compRatio{4.0f};        ///< Compressor ratio (e.g. 4:1)
    std::atomic<float> m_compAttack{5.0f};       ///< Attack in ms
    std::atomic<float> m_compRelease{100.0f};    ///< Release in ms
    std::atomic<float> m_gain{1.0f};             ///< Output gain

    // ── RT-only compressor envelope state ────────────────────────────────
    float m_compEnvelope = 0.0f;

    // ── Input device ID (Qt thread only) ─────────────────────────────────
    QString m_inputDeviceId;

    // ── Level meter (written in RT, read in UI thread) ────────────────────
    std::atomic<float> m_inputLevel{0.0f};

    PTTWidget* m_widget = nullptr;
};

} // namespace M1

Q_DECLARE_METATYPE(M1::PTTModule::State)

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_ptt_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_ptt_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_ptt_destroy(M1::IModule*);
}
