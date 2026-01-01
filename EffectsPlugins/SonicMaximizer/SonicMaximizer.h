#pragma once
#include "IEffectUnit.h"
#include "IPlugin.h"
#include <array>
#include <vector>
#include <atomic>

namespace M1 {

/// SonicMaximizer — BBE Sonic Maximizer clone.
///
/// Psychoacoustic enhancer that splits audio into three frequency bands,
/// applies phase correction and presence boost:
///
///   Low band  (< 150 Hz)  : 8-sample delay to align bass transients.
///   Mid band  (150–2500 Hz): slight attenuation to clean up the midrange.
///   High band (> 2500 Hz) : presence boost by m_process dB (0–10 dB).
///
/// Implemented using 1st-order IIR shelving filters.
/// All parameters are normalised 0.0–1.0 and map to physical values internally.
///
/// RT-safe: process() performs no allocation, no Qt API, no mutex wait.
class SonicMaximizer : public IEffectUnit {
public:
    SonicMaximizer();
    ~SonicMaximizer() override = default;

    // ── Identity ──────────────────────────────────────────────────────────
    QString effectId()    const override { return "com.mcaster1.fx.sonic"; }
    QString displayName() const override { return "Sonic Maximizer"; }
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

    // ── Parameter access (Qt thread) ──────────────────────────────────────
    void setLoContour(float v);  ///< 0.0–1.0
    void setProcess(float v);    ///< 0.0–1.0
    float loContour() const;
    float process()   const;

private:
    // ── First-order IIR shelf filter state ───────────────────────────────
    // For each of 2 channels we keep state for low-shelf and high-shelf filters.
    struct ShelfState {
        double z1 = 0.0;  ///< One sample delay state
    };

    struct ShelfCoeffs {
        double b0 = 1.0, b1 = 0.0;  ///< FIR taps
        double a1 = 0.0;             ///< IIR tap (z^-1 feedback)
        double gain = 1.0;           ///< linear post-gain
    };

    void computeCoefficients();

    // Low-shelf (isolate lows below 150 Hz) — then delay that signal 8 samples.
    ShelfCoeffs m_lowShelf;
    ShelfCoeffs m_highShelf;

    static constexpr int kMaxChannels = 8;
    static constexpr int kDelayLines  = 8;  ///< 8-sample bass alignment delay

    std::array<ShelfState, kMaxChannels> m_lowState{};
    std::array<ShelfState, kMaxChannels> m_highState{};

    // Circular delay buffer for low band (8 samples per channel).
    // Fixed-size, no heap allocation in process().
    std::array<std::array<float, kDelayLines>, kMaxChannels> m_delayBuf{};
    std::array<int, kMaxChannels>                            m_delayPos{};

    // Parameters (atomic so Qt-thread writes are visible to RT thread).
    std::atomic<float> m_loContour{0.5f};  ///< 0.0–1.0
    std::atomic<float> m_process{0.5f};    ///< 0.0–1.0

    double m_sampleRate = 48000.0;

    // Coefficients are recomputed on parameter change; guard with dirty flag.
    std::atomic<bool> m_coeffsDirty{true};
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_sonic_plugin_info();
    MCASTER1_PLUGIN_API M1::IEffectUnit*    mcaster1_fx_sonic_create(void* host);
    MCASTER1_PLUGIN_API void                mcaster1_fx_sonic_destroy(M1::IEffectUnit* fx);
}
