#include "AudioResampler.h"
#include <cmath>
#include <algorithm>

namespace M1 {

void AudioResampler::configure(double inputRate, double outputRate, int channels) {
    m_inputRate   = inputRate;
    m_outputRate  = outputRate;
    m_channels    = channels;
    m_ratio       = outputRate / inputRate;
    m_passThrough = (std::abs(m_ratio - 1.0) < 1e-6);
    m_phase       = 0.0;
}

int AudioResampler::process(const AudioBuffer& in, AudioBuffer& out) {
    if (!in.isValid || in.frames == 0) return 0;
    if (m_passThrough) {
        // Direct copy
        const int frames = std::min(in.frames, out.frames);
        for (int f = 0; f < frames; ++f)
            for (int ch = 0; ch < m_channels; ++ch)
                out.at(f, ch) = in.at(f, ch);
        return frames;
    }

    // Linear interpolation resampler
    int outFrame = 0;
    while (outFrame < out.frames) {
        double srcPos = outFrame / m_ratio + m_phase;
        int    i0     = static_cast<int>(srcPos);
        int    i1     = i0 + 1;
        float  frac   = static_cast<float>(srcPos - i0);

        if (i1 >= in.frames) break;

        for (int ch = 0; ch < m_channels && ch < out.channels; ++ch) {
            float s0 = in.at(i0, ch);
            float s1 = in.at(i1, ch);
            out.at(outFrame, ch) = s0 + frac * (s1 - s0);
        }
        ++outFrame;
    }
    // Update phase for next block (ensures continuity)
    m_phase = (outFrame / m_ratio) - std::floor(outFrame / m_ratio) * in.frames;
    return outFrame;
}

} // namespace M1
