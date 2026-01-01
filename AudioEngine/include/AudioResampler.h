#pragma once
#include "AudioBuffer.h"

namespace M1 {

/// Linear interpolation resampler for converting between device native rates
/// and engine target rate. Used when ASIO device runs at 96kHz but engine is 48kHz.
///
/// Phase 1: Simple linear interpolation (good enough for meter display).
/// Phase 2+: Replace with high-quality SRC (e.g. libsamplerate) for deck playback.
class AudioResampler {
public:
    AudioResampler() = default;
    ~AudioResampler() = default;

    /// Configure resampler.
    /// inputRate: source sample rate (e.g. 96000 from ASIO device)
    /// outputRate: target sample rate (e.g. 48000 for engine)
    /// channels: number of channels
    void configure(double inputRate, double outputRate, int channels);

    /// Resample inBuf → outBuf.
    /// outBuf must be pre-allocated with enough frames.
    /// Returns actual number of output frames written.
    int process(const AudioBuffer& in, AudioBuffer& out);

    double ratio() const { return m_ratio; }
    bool   isPassThrough() const { return m_passThrough; }

private:
    double m_inputRate  = 48000.0;
    double m_outputRate = 48000.0;
    double m_ratio      = 1.0;
    int    m_channels   = 2;
    bool   m_passThrough= true;
    double m_phase      = 0.0;
};

} // namespace M1
