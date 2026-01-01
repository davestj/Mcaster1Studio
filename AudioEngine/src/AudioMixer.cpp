#include "AudioMixer.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace M1 {

AudioMixer::AudioMixer() = default;

void AudioMixer::initialize(int channels, double sampleRate, int framesPerBuffer) {
    m_channels        = channels;
    m_sampleRate      = sampleRate;
    m_framesPerBuffer = framesPerBuffer;

    for (int c = 0; c < MAX_CHANNELS; ++c) {
        m_channelBufs[c].assign(static_cast<size_t>(channels * framesPerBuffer), 0.0f);
    }
}

void AudioMixer::mixIn(int channel, const AudioBuffer& src) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;
    if (!src.isValid || src.frames == 0) return;
    if (m_strips[channel].muted.load(std::memory_order_relaxed)) return;

    const float gain = m_strips[channel].gainLinear.load(std::memory_order_relaxed);
    const int frames = std::min(src.frames, m_framesPerBuffer);
    auto& buf = m_channelBufs[channel];

    float peakL = 0.0f, peakR = 0.0f;
    for (int f = 0; f < frames; ++f) {
        float l = src.at(f, 0) * gain;
        float r = (src.channels > 1) ? src.at(f, 1) * gain : l;
        buf[static_cast<size_t>(f * m_channels + 0)] += l;
        if (m_channels > 1)
            buf[static_cast<size_t>(f * m_channels + 1)] += r;
        peakL = std::max(peakL, std::abs(l));
        peakR = std::max(peakR, std::abs(r));
    }
    m_strips[channel].peakL.store(peakL, std::memory_order_relaxed);
    m_strips[channel].peakR.store(peakR, std::memory_order_relaxed);
}

void AudioMixer::renderMaster(AudioBuffer& out) {
    if (!out.isValid || out.frames == 0) return;
    const float masterGain = m_masterGain.load(std::memory_order_relaxed);
    const int frames = std::min(out.frames, m_framesPerBuffer);

    // Zero output
    out.silence();

    float peakL = 0.0f, peakR = 0.0f;
    for (int f = 0; f < frames; ++f) {
        float sumL = 0.0f, sumR = 0.0f;
        for (int c = 0; c < MAX_CHANNELS; ++c) {
            const auto& buf = m_channelBufs[c];
            sumL += buf[static_cast<size_t>(f * m_channels + 0)];
            if (m_channels > 1)
                sumR += buf[static_cast<size_t>(f * m_channels + 1)];
        }
        sumL *= masterGain;
        sumR *= masterGain;
        // Soft clip at ±1.0
        sumL = std::max(-1.0f, std::min(1.0f, sumL));
        sumR = std::max(-1.0f, std::min(1.0f, sumR));
        out.at(f, 0) = sumL;
        if (out.channels > 1) out.at(f, 1) = sumR;
        peakL = std::max(peakL, std::abs(sumL));
        peakR = std::max(peakR, std::abs(sumR));
    }
    m_masterPeakL.store(peakL, std::memory_order_relaxed);
    m_masterPeakR.store(peakR, std::memory_order_relaxed);

    // Clear accumulators
    for (int c = 0; c < MAX_CHANNELS; ++c)
        std::fill(m_channelBufs[c].begin(), m_channelBufs[c].end(), 0.0f);
}

void AudioMixer::setChannelGain(int channel, float gain) {
    if (channel >= 0 && channel < MAX_CHANNELS)
        m_strips[channel].gainLinear.store(gain, std::memory_order_relaxed);
}

void AudioMixer::setChannelMute(int channel, bool muted) {
    if (channel >= 0 && channel < MAX_CHANNELS)
        m_strips[channel].muted.store(muted, std::memory_order_relaxed);
}

void AudioMixer::setMasterGain(float gain) {
    m_masterGain.store(gain, std::memory_order_relaxed);
}

float AudioMixer::channelPeakL(int ch) const {
    return (ch >= 0 && ch < MAX_CHANNELS) ?
        m_strips[ch].peakL.load(std::memory_order_relaxed) : 0.0f;
}
float AudioMixer::channelPeakR(int ch) const {
    return (ch >= 0 && ch < MAX_CHANNELS) ?
        m_strips[ch].peakR.load(std::memory_order_relaxed) : 0.0f;
}
float AudioMixer::masterPeakL() const {
    return m_masterPeakL.load(std::memory_order_relaxed);
}
float AudioMixer::masterPeakR() const {
    return m_masterPeakR.load(std::memory_order_relaxed);
}

} // namespace M1
