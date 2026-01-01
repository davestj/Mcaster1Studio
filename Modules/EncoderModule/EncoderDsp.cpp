#include "EncoderDsp.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── configure ─────────────────────────────────────────────────────────────────
void EncoderDsp::configure(const EncoderConfig& cfg, int sampleRate)
{
    m_eqEnabled  = cfg.dspEnabled && (cfg.eqPreset != "flat");
    m_agcEnabled = cfg.dspEnabled && cfg.agcEnabled;
    m_pttEnabled = cfg.pttDuckEnabled;

    // Input gain
    m_inputGainLin = std::pow(10.f, cfg.agcInputGainDb / 20.f);

    // EQ
    if (m_eqEnabled)
        applyEqPreset(cfg.eqPreset, sampleRate);

    // AGC
    if (m_agcEnabled) {
        m_threshold  = std::pow(10.f, cfg.agcThresholdDb / 20.f);
        m_ratio      = cfg.agcRatio;
        m_makeupLin  = std::pow(10.f, cfg.agcMakeupGainDb / 20.f);
        m_limiterLin = std::pow(10.f, cfg.agcLimiterDb / 20.f);
        m_alphaA = std::exp(-1.f / (cfg.agcAttackMs  * 0.001f * static_cast<float>(sampleRate)));
        m_alphaR = std::exp(-1.f / (cfg.agcReleaseMs * 0.001f * static_cast<float>(sampleRate)));
        m_envL = m_envR = 0.f;
    }

    // PTT duck: 50ms ramp over 48000 Hz = 2400 frames; scale to sampleRate
    const int rampFrames = static_cast<int>(0.05f * static_cast<float>(sampleRate));
    m_pttAttenLin = std::pow(10.f, cfg.pttDuckAttenDb / 20.f);
    m_pttRampStep = (rampFrames > 0) ? (1.f - m_pttAttenLin) / static_cast<float>(rampFrames) : 1.f;
    m_pttGainCur  = 1.f;

    // Reset biquad state
    for (auto& s : m_eqState) {
        for (int c = 0; c < 2; ++c)
            s.x1[c] = s.x2[c] = s.y1[c] = s.y2[c] = 0.f;
    }
}

// ── process ───────────────────────────────────────────────────────────────────
void EncoderDsp::process(float* pcm, int frames, int channels)
{
    // Input gain (pre-AGC)
    if (m_agcEnabled && m_inputGainLin != 1.f) {
        const int total = frames * channels;
        for (int i = 0; i < total; ++i)
            pcm[i] *= m_inputGainLin;
    }

    if (m_eqEnabled)
        for (int b = 0; b < kBands; ++b)
            processEqBand(b, pcm, frames, channels);

    if (m_agcEnabled)
        processAgc(pcm, frames, channels);

    if (m_pttEnabled)
        processPttDuck(pcm, frames, channels);
}

// ── setPttActive ──────────────────────────────────────────────────────────────
void EncoderDsp::setPttActive(bool active)
{
    m_pttActive.store(active, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// EQ preset application
// ─────────────────────────────────────────────────────────────────────────────
void EncoderDsp::applyEqPreset(const QString& preset, int sampleRate)
{
    const float sr = static_cast<float>(sampleRate);

    // Default all to unity (flat biquad)
    for (int b = 0; b < kBands; ++b)
        m_eqCoeffs[b] = {};   // b0=1, rest=0 → pass-through

    // Frequencies: 32, 64, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
    constexpr float fc[kBands] = {32,64,125,250,500,1000,2000,4000,8000,16000};
    constexpr float Q = 0.707f;

    if (preset == "broadcast") {
        // Gentle radio curve: slight bass roll-off, mid presence, air top
        m_eqCoeffs[0] = makeLowShelf (fc[0], -2.f, sr);
        m_eqCoeffs[1] = makePeaking  (fc[1], -1.f, Q, sr);
        m_eqCoeffs[4] = makePeaking  (fc[4], +1.f, Q, sr);
        m_eqCoeffs[6] = makePeaking  (fc[6], +2.f, Q, sr);
        m_eqCoeffs[7] = makePeaking  (fc[7], +1.5f,Q, sr);
        m_eqCoeffs[9] = makeHighShelf(fc[9], +1.f, sr);
    }
    else if (preset == "spoken_word") {
        // Voice-optimised: reduce rumble, boost 2-4 kHz presence
        m_eqCoeffs[0] = makeLowShelf (fc[0], -4.f, sr);
        m_eqCoeffs[1] = makePeaking  (fc[1], -2.f, Q, sr);
        m_eqCoeffs[2] = makePeaking  (fc[2], -1.f, Q, sr);
        m_eqCoeffs[6] = makePeaking  (fc[6], +3.f, Q, sr);
        m_eqCoeffs[7] = makePeaking  (fc[7], +2.5f,Q, sr);
        m_eqCoeffs[8] = makePeaking  (fc[8], +1.f, Q, sr);
        m_eqCoeffs[9] = makeHighShelf(fc[9], -1.f, sr);
    }
    else if (preset == "classic_rock") {
        // Lifted bass + 4 kHz presence
        m_eqCoeffs[0] = makeLowShelf (fc[0], +3.f, sr);
        m_eqCoeffs[1] = makePeaking  (fc[1], +2.f, Q, sr);
        m_eqCoeffs[2] = makePeaking  (fc[2], +1.f, Q, sr);
        m_eqCoeffs[7] = makePeaking  (fc[7], +2.f, Q, sr);
        m_eqCoeffs[8] = makePeaking  (fc[8], +1.f, Q, sr);
    }
    else if (preset == "country") {
        // Warmth lift, slight treble roll
        m_eqCoeffs[0] = makeLowShelf (fc[0], +1.5f, sr);
        m_eqCoeffs[2] = makePeaking  (fc[2], +1.f, Q, sr);
        m_eqCoeffs[9] = makeHighShelf(fc[9], -1.5f, sr);
    }
    else if (preset == "modern_rock") {
        // Mid-scoop + bright top
        m_eqCoeffs[0] = makeLowShelf (fc[0], +2.f, sr);
        m_eqCoeffs[4] = makePeaking  (fc[4], -2.f, Q, sr);
        m_eqCoeffs[5] = makePeaking  (fc[5], -1.5f,Q, sr);
        m_eqCoeffs[8] = makePeaking  (fc[8], +2.f, Q, sr);
        m_eqCoeffs[9] = makeHighShelf(fc[9], +2.f, sr);
    }
    // "flat" — all pass-through (already set above)
}

// ── RBJ biquad coefficient helpers ───────────────────────────────────────────
EncoderDsp::BiquadCoeffs EncoderDsp::makeLowShelf(float fc, float gainDb, float sampleRate)
{
    const float A  = std::pow(10.f, gainDb / 40.f);
    const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
    const float cosW = std::cos(w0);
    const float S  = 1.f;
    const float al = std::sin(w0) / 2.f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);
    const float sq = 2.f * std::sqrt(A) * al;

    BiquadCoeffs c;
    const float a0 = (A+1) + (A-1)*cosW + sq;
    c.b0 = (A*((A+1) - (A-1)*cosW + sq)) / a0;
    c.b1 = (2*A*((A-1) - (A+1)*cosW))    / a0;
    c.b2 = (A*((A+1) - (A-1)*cosW - sq)) / a0;
    c.a1 = (-2*((A-1) + (A+1)*cosW))     / a0;
    c.a2 = ((A+1) + (A-1)*cosW - sq)     / a0;
    return c;
}

EncoderDsp::BiquadCoeffs EncoderDsp::makePeaking(float fc, float gainDb, float Q, float sampleRate)
{
    const float A  = std::pow(10.f, gainDb / 40.f);
    const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
    const float al = std::sin(w0) / (2.f * Q);

    BiquadCoeffs c;
    const float a0 = 1 + al/A;
    c.b0 = (1 + al*A) / a0;
    c.b1 = (-2*std::cos(w0)) / a0;
    c.b2 = (1 - al*A) / a0;
    c.a1 = (-2*std::cos(w0)) / a0;
    c.a2 = (1 - al/A)        / a0;
    return c;
}

EncoderDsp::BiquadCoeffs EncoderDsp::makeHighShelf(float fc, float gainDb, float sampleRate)
{
    const float A  = std::pow(10.f, gainDb / 40.f);
    const float w0 = 2.f * static_cast<float>(M_PI) * fc / sampleRate;
    const float cosW = std::cos(w0);
    const float S  = 1.f;
    const float al = std::sin(w0) / 2.f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);
    const float sq = 2.f * std::sqrt(A) * al;

    BiquadCoeffs c;
    const float a0 = (A+1) - (A-1)*cosW + sq;
    c.b0 = (A*((A+1) + (A-1)*cosW + sq)) / a0;
    c.b1 = (-2*A*((A-1) + (A+1)*cosW))   / a0;
    c.b2 = (A*((A+1) + (A-1)*cosW - sq)) / a0;
    c.a1 = (2*((A-1) - (A+1)*cosW))      / a0;
    c.a2 = ((A+1) - (A-1)*cosW - sq)     / a0;
    return c;
}

// ── EQ band processing ────────────────────────────────────────────────────────
void EncoderDsp::processEqBand(int b, float* pcm, int frames, int channels)
{
    const BiquadCoeffs& c = m_eqCoeffs[b];
    BiquadState& s        = m_eqState[b];
    const int ch          = std::min(channels, 2);

    for (int f = 0; f < frames; ++f) {
        for (int ci = 0; ci < ch; ++ci) {
            const float x = pcm[f * channels + ci];
            const float y = c.b0*x + c.b1*s.x1[ci] + c.b2*s.x2[ci]
                                   - c.a1*s.y1[ci]  - c.a2*s.y2[ci];
            s.x2[ci] = s.x1[ci]; s.x1[ci] = x;
            s.y2[ci] = s.y1[ci]; s.y1[ci] = y;
            pcm[f * channels + ci] = y;
        }
    }
}

// ── AGC processing ────────────────────────────────────────────────────────────
void EncoderDsp::processAgc(float* pcm, int frames, int channels)
{
    const bool stereo = (channels >= 2);

    for (int f = 0; f < frames; ++f) {
        const float xL = pcm[f * channels + 0];
        const float xR = stereo ? pcm[f * channels + 1] : xL;

        // Envelope follower (peak detector)
        const float peakL = std::fabs(xL);
        const float peakR = std::fabs(xR);
        m_envL = (peakL > m_envL) ? m_alphaA * m_envL + (1.f - m_alphaA) * peakL
                                  : m_alphaR * m_envL;
        m_envR = (peakR > m_envR) ? m_alphaA * m_envR + (1.f - m_alphaA) * peakR
                                  : m_alphaR * m_envR;

        const float env = std::max(m_envL, m_envR);

        // Soft-knee compression gain
        float gainLin = 1.f;
        if (env > m_threshold) {
            const float overDb = 20.f * std::log10(env / m_threshold);
            const float reduDb  = overDb * (1.f - 1.f / m_ratio);
            gainLin = std::pow(10.f, -reduDb / 20.f);
        }
        gainLin *= m_makeupLin;

        // Apply + hard limiter
        float outL = xL * gainLin;
        float outR = xR * gainLin;
        outL = std::clamp(outL, -m_limiterLin, m_limiterLin);
        outR = std::clamp(outR, -m_limiterLin, m_limiterLin);

        pcm[f * channels + 0] = outL;
        if (stereo) pcm[f * channels + 1] = outR;
    }
}

// ── PTT duck processing ───────────────────────────────────────────────────────
void EncoderDsp::processPttDuck(float* pcm, int frames, int channels)
{
    const float target = m_pttActive.load(std::memory_order_relaxed)
                         ? m_pttAttenLin : 1.f;

    for (int f = 0; f < frames; ++f) {
        // Ramp toward target
        if (m_pttGainCur < target)
            m_pttGainCur = std::min(m_pttGainCur + m_pttRampStep, target);
        else if (m_pttGainCur > target)
            m_pttGainCur = std::max(m_pttGainCur - m_pttRampStep, target);

        for (int c = 0; c < channels; ++c)
            pcm[f * channels + c] *= m_pttGainCur;
    }
}
