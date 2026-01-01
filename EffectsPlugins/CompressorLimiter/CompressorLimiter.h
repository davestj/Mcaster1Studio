#pragma once
#include "IEffectUnit.h"
#include "IPlugin.h"
#include <atomic>
#include <array>

namespace M1 {

/// CompressorLimiter — Stereo RMS compressor with integrated brickwall limiter.
///
/// Algorithm:
///   1. RMS level detection over a short window (per-channel, interleaved).
///   2. Gain computer: soft-knee gain reduction using threshold + ratio.
///   3. Smooth gain application via attack / release envelope.
///   4. Makeup gain applied linearly.
///   5. Optional brickwall limiter (ceiling = -0.3 dBFS).
///
/// All parameters are changed from the Qt thread; read atomically from RT thread.
/// Peak L/R levels (post-makeup) are written by the RT thread and read by a
/// QTimer-driven UI refresh for the gain-reduction meter.
///
/// RT-safe: process() uses no allocation, no Qt calls, no mutex.
class CompressorLimiter : public IEffectUnit {
public:
    CompressorLimiter();
    ~CompressorLimiter() override = default;

    // ── Identity ──────────────────────────────────────────────────────────
    QString effectId()    const override { return "com.mcaster1.fx.comp"; }
    QString displayName() const override { return "Compressor/Limiter"; }
    QString vendor()      const override { return "Mcaster1"; }
    int     rackUnits()   const override { return 1; }

    // ── Lifecycle ─────────────────────────────────────────────────────────
    void initialize(double sampleRate, int maxFramesPerBuffer) override;
    void reset()                                               override;

    // ── DSP (RT thread) ───────────────────────────────────────────────────
    void process(AudioBuffer& inOut) override;

    // ── UI ────────────────────────────────────────────────────────────────
    QWidget* createPanel(QWidget* parent) override;

    // ── State ─────────────────────────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Parameters (set from Qt thread) ───────────────────────────────────
    void  setThreshold(float dBFS);      ///< -40 to 0 dBFS
    void  setRatio(float ratio);         ///< 1.0 to 20.0
    void  setAttack(float ms);           ///< 0.1 to 100 ms
    void  setRelease(float ms);          ///< 50 to 2000 ms
    void  setMakeupGain(float dB);       ///< 0 to 20 dB
    void  setKnee(float dB);             ///< 0 to 6 dB (soft-knee width)
    void  setLimiterEnabled(bool en);    ///< Brickwall at -0.3 dBFS

    float threshold()       const;
    float ratio()           const;
    float attackMs()        const;
    float releaseMs()       const;
    float makeupGain()      const;
    float knee()            const;
    bool  limiterEnabled()  const;

    // ── Metering (read from Qt thread via QTimer) ─────────────────────────
    float peakLeft()          const;  ///< Most recent peak level (linear)
    float peakRight()         const;
    float gainReductionDb()   const;  ///< Current gain reduction in dB (>= 0)

private:
    void recomputeEnvelopes();

    // Parameters.
    std::atomic<float> m_threshold{-18.0f};   ///< dBFS
    std::atomic<float> m_ratio{4.0f};
    std::atomic<float> m_attackMs{10.0f};
    std::atomic<float> m_releaseMs{200.0f};
    std::atomic<float> m_makeupGainDb{6.0f};
    std::atomic<float> m_kneeDb{3.0f};
    std::atomic<bool>  m_limiterEnabled{true};

    // Envelope coefficients (recomputed when parameters change).
    std::atomic<double> m_attackCoeff{0.0};
    std::atomic<double> m_releaseCoeff{0.0};
    std::atomic<bool>   m_envDirty{true};

    double m_sampleRate = 48000.0;

    // ── Per-channel state (RT thread only) ────────────────────────────────
    static constexpr int kMaxChannels = 2;  ///< Stereo
    static constexpr int kRmsWindow   = 256; ///< RMS window in samples

    // RMS accumulators.
    std::array<double, kMaxChannels> m_rmsAccum{};
    std::array<int,    kMaxChannels> m_rmsCount{};

    // Gain smoothing envelope.
    double m_gainDb = 0.0;  ///< Current smoothed gain (dB, <= 0)

    // RMS ring buffer for level detection.
    static constexpr int kRmsBufSize = 512;
    std::array<std::array<float, kRmsBufSize>, kMaxChannels> m_rmsBuf{};
    std::array<int, kMaxChannels>                             m_rmsBufPos{};
    std::array<double, kMaxChannels>                          m_rmsSumSq{};

    // ── Metering (atomic, written RT, read Qt) ────────────────────────────
    std::atomic<float> m_peakLeft{0.0f};
    std::atomic<float> m_peakRight{0.0f};
    std::atomic<float> m_gainReductionDb{0.0f};

    // Limiter ceiling (-0.3 dBFS).
    static constexpr float kLimiterCeiling = 0.9659f;  ///< 10^(-0.3/20)
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_comp_plugin_info();
    MCASTER1_PLUGIN_API M1::IEffectUnit*    mcaster1_fx_comp_create(void* host);
    MCASTER1_PLUGIN_API void                mcaster1_fx_comp_destroy(M1::IEffectUnit* fx);
}
