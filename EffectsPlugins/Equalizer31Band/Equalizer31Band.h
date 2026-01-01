#pragma once
#include "IEffectUnit.h"
#include "IPlugin.h"
#include <array>
#include <atomic>

namespace M1 {

/// Equalizer31Band — ISO 31-band graphic equalizer.
///
/// 31 peak/notch biquad filters at ISO standard center frequencies.
/// Gain range: ±12 dB per band.  Q = 4.0 (1/3 octave).
///
/// Biquad coefficients computed using the Audio EQ Cookbook
/// (Robert Bristow-Johnson) peaking EQ filter formulas.
///
/// RT-safe: process() applies all 31 biquads in sequence per sample.
/// No allocation, no Qt calls, no mutex inside process().
class Equalizer31Band : public IEffectUnit {
public:
    static constexpr int kBands = 31;

    /// ISO 31-band standard center frequencies (Hz).
    static constexpr std::array<double, kBands> kCenterFreqs = {
        20.0,   25.0,   31.5,   40.0,   50.0,
        63.0,   80.0,  100.0,  125.0,  160.0,
       200.0,  250.0,  315.0,  400.0,  500.0,
       630.0,  800.0, 1000.0, 1250.0, 1600.0,
      2000.0, 2500.0, 3150.0, 4000.0, 5000.0,
      6300.0, 8000.0,10000.0,12500.0,16000.0,
     20000.0
    };

    Equalizer31Band();
    ~Equalizer31Band() override = default;

    // ── Identity ──────────────────────────────────────────────────────────
    QString effectId()    const override { return "com.mcaster1.fx.eq31"; }
    QString displayName() const override { return "31-Band EQ"; }
    QString vendor()      const override { return "Mcaster1"; }
    int     rackUnits()   const override { return 2; }

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

    // ── Parameter access (Qt thread) ──────────────────────────────────────
    /// Set band gain in dB [-12, +12].
    void  setBandGain(int band, float dB);
    float bandGain(int band) const;

private:
    // ── Biquad coefficients per band ─────────────────────────────────────
    struct BiquadCoeffs {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;  ///< Numerator
        double a1 = 0.0, a2 = 0.0;             ///< Denominator (a0 = 1.0)
    };

    // ── Per-channel biquad filter state (direct form II transposed) ───────
    // State per [band][channel].
    static constexpr int kMaxChannels = 8;
    struct BiquadState {
        double w1 = 0.0, w2 = 0.0;
    };

    std::array<BiquadCoeffs, kBands> m_coeffs{};
    std::array<std::array<BiquadState, kMaxChannels>, kBands> m_state{};

    // Band gains stored atomically (Qt thread writes, RT thread reads).
    // Note: std::atomic is not copy/move constructible; initialized in constructor.
    std::array<std::atomic<float>, kBands> m_gainDb;

    // Dirty flags: true when coefficients need recompute.
    std::array<std::atomic<bool>, kBands> m_dirty;

    double m_sampleRate = 48000.0;

    void computeBandCoeffs(int band);

    static constexpr double kQ = 4.0;  ///< 1/3-octave Q
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_eq31_plugin_info();
    MCASTER1_PLUGIN_API M1::IEffectUnit*    mcaster1_fx_eq31_create(void* host);
    MCASTER1_PLUGIN_API void                mcaster1_fx_eq31_destroy(M1::IEffectUnit* fx);
}
