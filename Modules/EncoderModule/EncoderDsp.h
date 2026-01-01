#pragma once
#include "EncoderConfig.h"
#include <atomic>

/// EncoderDsp — per-slot DSP chain for the encoder module.
///
/// Processes PCM **in the encoder thread** (not the RT audio thread), immediately
/// before encoding each frame. Chain order:
///   Input Gain → 10-band parametric EQ (preset) → feedforward AGC + hard limiter → PTT duck
///
/// All filter coefficients are precomputed once in configure() — process() does
/// no allocation and no mutex waits, making it safe to call at high frequency
/// from the encoder thread.
///
/// PTT duck state is toggled from the main thread (via setPttActive()) and is
/// applied with a 50 ms linear fade to avoid clicks on the encoded stream.
class EncoderDsp {
public:
    EncoderDsp() = default;

    /// Configure the DSP chain from an EncoderConfig.
    /// Call once before starting the encode loop.
    void configure(const EncoderConfig& cfg, int sampleRate);

    /// Process PCM in-place.
    /// @param pcm      interleaved float32 samples
    /// @param frames   number of audio frames (samples per channel)
    /// @param channels number of channels (1 or 2)
    void process(float* pcm, int frames, int channels);

    /// Set PTT active state (called from main thread).
    /// Internally uses std::atomic so the encoder thread reads it safely.
    void setPttActive(bool active);

private:
    // ── Biquad EQ ─────────────────────────────────────────────────────────────
    static constexpr int kBands = 10;

    struct BiquadCoeffs {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f;  // numerator
        float a1 = 0.f, a2 = 0.f;             // denominator (a0 normalised to 1)
    };

    // Per-channel state (stereo = 2 channels)
    struct BiquadState {
        float x1[2] = {}, x2[2] = {};  // [channel]
        float y1[2] = {}, y2[2] = {};
    };

    BiquadCoeffs m_eqCoeffs[kBands] = {};
    BiquadState  m_eqState[kBands]  = {};
    bool         m_eqEnabled        = false;

    /// Compute biquad coefficients for the named EQ preset.
    void applyEqPreset(const QString& preset, int sampleRate);

    /// Low-shelf, peaking, or high-shelf RBJ biquad coefficient computation.
    static BiquadCoeffs makeLowShelf (float fc, float gainDb, float sampleRate);
    static BiquadCoeffs makePeaking  (float fc, float gainDb, float Q, float sampleRate);
    static BiquadCoeffs makeHighShelf(float fc, float gainDb, float sampleRate);

    /// Process one EQ band in-place.
    void processEqBand(int band, float* pcm, int frames, int channels);

    // ── AGC (feedforward compressor + hard limiter) ───────────────────────────
    bool  m_agcEnabled   = false;
    float m_inputGainLin = 1.f;    // linear gain from agcInputGainDb
    float m_threshold    = 0.f;    // linear threshold
    float m_ratio        = 4.f;
    float m_knee         = 6.f;    // dB soft-knee width (fixed at 6 dB)
    float m_makeupLin    = 1.f;    // makeup gain linear
    float m_limiterLin   = 1.f;    // hard limiter ceiling linear

    // Per-channel envelope state
    float m_envL = 0.f, m_envR = 0.f;

    float m_alphaA = 0.f;  // precomputed attack  coeff: exp(-1/(attack_ms*0.001*sr))
    float m_alphaR = 0.f;  // precomputed release coeff

    void processAgc(float* pcm, int frames, int channels);

    // ── PTT duck ──────────────────────────────────────────────────────────────
    bool               m_pttEnabled   = false;
    float              m_pttAttenLin  = 1.f;   // target linear gain when active
    std::atomic<bool>  m_pttActive    {false};  // set from main thread
    float              m_pttGainCur   = 1.f;   // current smoothed gain (encoder thread)
    float              m_pttRampStep  = 0.f;   // gain step per frame for 50ms ramp

    void processPttDuck(float* pcm, int frames, int channels);
};
