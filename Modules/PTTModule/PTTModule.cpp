#include "PTTModule.h"
#include "PTTWidget.h"
#include "AudioBuffer.h"
#include "IPlugin.h"
#include <QSettings>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PTTModule::PTTModule(QObject* parent)
    : IModule(parent)
    , m_compEnvelope(0.0f)
{
    qRegisterMetaType<M1::PTTModule::State>("M1::PTTModule::State");
}

PTTModule::~PTTModule() {
    shutdown();
}

// ─── IModule lifecycle ────────────────────────────────────────────────────────

void PTTModule::initialize() {
    qInfo() << "[PTTModule] Initialized — state: Off";
}

void PTTModule::shutdown() {
    setState(State::Off);
    qInfo() << "[PTTModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────

QWidget* PTTModule::createWidget(QWidget* parent) {
    m_widget = new PTTWidget(this, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    return m_widget;
}

// ─── State control (Qt thread) ────────────────────────────────────────────────

void PTTModule::setState(State s) {
    m_state.store(static_cast<int>(s), std::memory_order_relaxed);
    emit stateChanged(s);
}

PTTModule::State PTTModule::state() const {
    return static_cast<State>(m_state.load(std::memory_order_relaxed));
}

// ─── DSP helpers (RT thread only) ────────────────────────────────────────────

float PTTModule::computeRms(const float* data, int frames, int channels, int ch) const noexcept {
    if (frames <= 0 || !data) return 0.0f;
    float sum = 0.0f;
    for (int f = 0; f < frames; ++f) {
        const float s = data[f * channels + ch];
        sum += s * s;
    }
    return std::sqrt(sum / static_cast<float>(frames));
}

// Approximate band energy using first-order IIR bandpass (RT-safe, no alloc).
// We use a simple two-pole resonator approximation by high-passing at freqLow
// and low-passing at freqHigh and computing RMS of the result.
float PTTModule::computeBandEnergy(const float* data, int frames, int channels,
                                    int ch, double sampleRate,
                                    float freqLow, float freqHigh) const noexcept {
    if (frames <= 0 || !data) return 0.0f;

    // First-order high-pass coefficient
    const float omegaH  = 2.0f * static_cast<float>(M_PI) * freqLow / static_cast<float>(sampleRate);
    const float alphaH  = 1.0f - omegaH / (omegaH + 1.0f);

    // First-order low-pass coefficient
    const float omegaL  = 2.0f * static_cast<float>(M_PI) * freqHigh / static_cast<float>(sampleRate);
    const float alphaL  = omegaL / (omegaL + 1.0f);

    float hpPrev = 0.0f, hpOut = 0.0f;
    float lpPrev = 0.0f;
    float sum    = 0.0f;

    for (int f = 0; f < frames; ++f) {
        const float x = data[f * channels + ch];
        // High-pass: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
        hpOut = alphaH * (hpOut + x - hpPrev);
        hpPrev = x;
        // Low-pass on HP output
        lpPrev = lpPrev + alphaL * (hpOut - lpPrev);
        sum += lpPrev * lpPrev;
    }
    return std::sqrt(sum / static_cast<float>(frames));
}

// First-order low-shelf boost in-place (RT-safe).
// fc: shelf corner frequency, gainDb: positive = boost
void PTTModule::applyLowShelfBoost(float* data, int frames, int channels,
                                    int ch, double sampleRate, float fc, float gainDb) noexcept {
    if (frames <= 0 || !data || gainDb == 0.0f) return;

    const float A      = std::pow(10.0f, gainDb / 40.0f);  // sqrt of linear gain
    const float omega  = 2.0f * static_cast<float>(M_PI) * fc / static_cast<float>(sampleRate);
    const float sinW   = std::sin(omega);
    const float cosW   = std::cos(omega);
    const float beta   = std::sqrt(A) / 1.0f;  // slope=1

    const float b0 =  A * ((A + 1.0f) - (A - 1.0f) * cosW + beta * sinW);
    const float b1 =  2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
    const float b2 =  A * ((A + 1.0f) - (A - 1.0f) * cosW - beta * sinW);
    const float a0 =       (A + 1.0f) + (A - 1.0f) * cosW + beta * sinW;
    const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
    const float a2 =       (A + 1.0f) + (A - 1.0f) * cosW - beta * sinW;

    const float inv_a0 = 1.0f / a0;
    const float nb0 = b0 * inv_a0;
    const float nb1 = b1 * inv_a0;
    const float nb2 = b2 * inv_a0;
    const float na1 = a1 * inv_a0;
    const float na2 = a2 * inv_a0;

    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    for (int f = 0; f < frames; ++f) {
        float x0 = data[f * channels + ch];
        float y0 = nb0 * x0 + nb1 * x1 + nb2 * x2 - na1 * y1 - na2 * y2;
        x2 = x1; x1 = x0;
        y2 = y1; y1 = y0;
        data[f * channels + ch] = y0;
    }
}

// First-order high-shelf cut in-place (RT-safe).
// gainDb: negative = cut
void PTTModule::applyHighShelfCut(float* data, int frames, int channels,
                                   int ch, double sampleRate, float fc, float gainDb) noexcept {
    if (frames <= 0 || !data || gainDb == 0.0f) return;

    const float A      = std::pow(10.0f, gainDb / 40.0f);
    const float omega  = 2.0f * static_cast<float>(M_PI) * fc / static_cast<float>(sampleRate);
    const float sinW   = std::sin(omega);
    const float cosW   = std::cos(omega);
    const float beta   = std::sqrt(A) / 1.0f;

    const float b0 =  A * ((A + 1.0f) + (A - 1.0f) * cosW + beta * sinW);
    const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
    const float b2 =  A * ((A + 1.0f) + (A - 1.0f) * cosW - beta * sinW);
    const float a0 =       (A + 1.0f) - (A - 1.0f) * cosW + beta * sinW;
    const float a1 =  2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
    const float a2 =       (A + 1.0f) - (A - 1.0f) * cosW - beta * sinW;

    const float inv_a0 = 1.0f / a0;
    const float nb0 = b0 * inv_a0;
    const float nb1 = b1 * inv_a0;
    const float nb2 = b2 * inv_a0;
    const float na1 = a1 * inv_a0;
    const float na2 = a2 * inv_a0;

    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    for (int f = 0; f < frames; ++f) {
        float x0 = data[f * channels + ch];
        float y0 = nb0 * x0 + nb1 * x1 + nb2 * x2 - na1 * y1 - na2 * y2;
        x2 = x1; x1 = x0;
        y2 = y1; y1 = y0;
        data[f * channels + ch] = y0;
    }
}

// ─── RT audio thread ──────────────────────────────────────────────────────────
//
// DSP chain per channel:
//   1. Noise gate   — silence output if RMS < gateThreshold
//   2. De-esser     — attenuate 5–8 kHz band energy when it exceeds threshold
//   3. Compressor   — peak envelope compressor (attack/release coefficients)
//   4. EQ shelf     — low shelf boost at 80 Hz, high shelf cut at 12 kHz
//
// Result is added into `out` only when state == Live.
// Level meter (m_inputLevel) is always updated from `in` regardless of state.
// NO Qt APIs, NO allocation, NO mutex waits.

void PTTModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    if (!in.isValid || in.frames <= 0) return;

    const int   frames     = in.frames;
    const int   channels   = in.channels;
    const float sampleRate = static_cast<float>(in.sampleRate);

    // ── Update input level meter (always, from raw input) ────────────────
    {
        float peak = 0.0f;
        const int total = frames * channels;
        for (int i = 0; i < total; ++i) {
            const float abs_s = std::fabs(in.data[i]);
            if (abs_s > peak) peak = abs_s;
        }
        // Simple 10ms attack / 200ms release envelope
        const float attackCoeff  = std::exp(-1.0f / (0.010f * sampleRate));
        const float releaseCoeff = std::exp(-1.0f / (0.200f * sampleRate));
        float cur = m_inputLevel.load(std::memory_order_relaxed);
        cur = (peak > cur) ? attackCoeff * cur + (1.0f - attackCoeff) * peak
                           : releaseCoeff * cur;
        m_inputLevel.store(cur, std::memory_order_relaxed);
    }

    // ── Only mix into output when Live ───────────────────────────────────
    const int s = m_state.load(std::memory_order_relaxed);
    if (s != static_cast<int>(State::Live)) return;

    // ── Load DSP params (single load per block — relaxed is fine) ────────
    const float gateThresh  = m_gateThreshold.load(std::memory_order_relaxed);
    const float deEssAmt    = m_deEssAmount.load(std::memory_order_relaxed);
    const float compThresh  = m_compThreshold.load(std::memory_order_relaxed);
    const float compRatio   = m_compRatio.load(std::memory_order_relaxed);
    const float compAttkMs  = m_compAttack.load(std::memory_order_relaxed);
    const float compRelMs   = m_compRelease.load(std::memory_order_relaxed);
    const float gain        = m_gain.load(std::memory_order_relaxed);

    const float compAttkC   = std::exp(-1.0f / (compAttkMs * 0.001f * sampleRate));
    const float compRelC    = std::exp(-1.0f / (compRelMs  * 0.001f * sampleRate));

    // Work on a local scratch buffer built from in.data (no allocation — use out scratch).
    // We mix into out, so we must process per-channel then add.
    // Strategy: apply DSP in-place on in.data is not allowed (const-logical).
    // Use out as a scratch add buffer — iterate and add processed sample directly.

    for (int ch = 0; ch < channels && ch < (out.isValid ? out.channels : 0); ++ch) {
        // ── 1. Noise gate — per channel ──────────────────────────────────
        const float rms = computeRms(in.data, frames, channels, ch);
        if (rms < gateThresh) continue; // gate closed — skip this channel

        // ── 2. De-esser — detect 5-8kHz band energy ──────────────────────
        const float bandEnergy = computeBandEnergy(in.data, frames, channels, ch,
                                                   in.sampleRate, 5000.0f, 8000.0f);
        // If band energy exceeds a fixed threshold (relative to rms), attenuate
        const float deEssGain = (bandEnergy > rms * 0.6f)
                                ? (1.0f - deEssAmt * std::min(1.0f, (bandEnergy / (rms + 1e-9f) - 0.6f)))
                                : 1.0f;

        // ── 3. Compressor + EQ + mix into out ────────────────────────────
        for (int f = 0; f < frames; ++f) {
            float smp = in.data[f * channels + ch] * deEssGain;

            // Peak compressor (simple feed-forward)
            const float absSmp = std::fabs(smp);
            if (absSmp > m_compEnvelope)
                m_compEnvelope = compAttkC * m_compEnvelope + (1.0f - compAttkC) * absSmp;
            else
                m_compEnvelope = compRelC  * m_compEnvelope + (1.0f - compRelC)  * absSmp;

            if (m_compEnvelope > compThresh && compRatio > 1.0f) {
                const float overDb  = 20.0f * std::log10(m_compEnvelope / compThresh + 1e-30f);
                const float gainRed = overDb * (1.0f - 1.0f / compRatio);
                smp *= std::pow(10.0f, -gainRed / 20.0f);
            }

            // Apply output gain
            smp *= gain;

            // Add to output (mix — do not overwrite)
            if (out.isValid && f < out.frames)
                out.data[f * out.channels + ch] += smp;
        }

        // ── 4. EQ shelf — applied post-mix (approximate — apply to out contribution)
        // We approximate shelf by post-processing the contributed samples.
        // Since we already mixed in, apply correction pass: low shelf boost at 80Hz,
        // high shelf cut at 12kHz. These are applied directly into out.data.
        // NOTE: This is a simplification since we operate on the full out buffer.
        // A proper implementation would use a per-module scratch buffer.
        // For correctness we apply to the last `frames` frames added to out[ch].
        if (out.isValid) {
            applyLowShelfBoost(out.data, std::min(frames, out.frames),
                               out.channels, ch, in.sampleRate, 80.0f, 3.0f);
            applyHighShelfCut(out.data, std::min(frames, out.frames),
                              out.channels, ch, in.sampleRate, 12000.0f, -2.5f);
        }
    }
}

// ─── State persistence ────────────────────────────────────────────────────────

void PTTModule::saveState(QSettings& s) {
    s.beginGroup("PTTModule");
    s.setValue("gateThreshold",  static_cast<double>(m_gateThreshold.load()));
    s.setValue("deEssAmount",    static_cast<double>(m_deEssAmount.load()));
    s.setValue("compThreshold",  static_cast<double>(m_compThreshold.load()));
    s.setValue("compRatio",      static_cast<double>(m_compRatio.load()));
    s.setValue("compAttack",     static_cast<double>(m_compAttack.load()));
    s.setValue("compRelease",    static_cast<double>(m_compRelease.load()));
    s.setValue("gain",           static_cast<double>(m_gain.load()));
    s.setValue("inputDeviceId",  m_inputDeviceId);
    s.endGroup();
}

void PTTModule::loadState(QSettings& s) {
    s.beginGroup("PTTModule");
    m_gateThreshold.store(s.value("gateThreshold", 0.01).toFloat());
    m_deEssAmount.store(  s.value("deEssAmount",   0.5).toFloat());
    m_compThreshold.store(s.value("compThreshold", 0.7).toFloat());
    m_compRatio.store(    s.value("compRatio",      4.0).toFloat());
    m_compAttack.store(   s.value("compAttack",     5.0).toFloat());
    m_compRelease.store(  s.value("compRelease",  100.0).toFloat());
    m_gain.store(         s.value("gain",           1.0).toFloat());
    m_inputDeviceId = s.value("inputDeviceId").toString();
    s.endGroup();
}

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────

static Mcaster1PluginInfo s_pttInfo {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.ptt",
    "PTT Mic",
    "1.0.0",
    "podcast,church",
    "module",
    "Mcaster1",
    "Push-to-Talk microphone with noise gate, de-esser, compressor and EQ"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_ptt_plugin_info() {
    return &s_pttInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_ptt_create(IModuleHost* /*host*/) {
    return new M1::PTTModule();
}
MCASTER1_PLUGIN_API void mcaster1_ptt_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
