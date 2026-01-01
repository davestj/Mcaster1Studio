#pragma once
#include "AudioBuffer.h"
#include <vector>
#include <atomic>

namespace M1 {

/// Per-channel strip state (accessed from RT thread — use atomics only)
struct ChannelStrip {
    std::atomic<float> gainLinear   {1.0f};   ///< 0.0 = mute, 1.0 = unity
    std::atomic<bool>  muted        {false};
    std::atomic<float> peakL        {0.0f};
    std::atomic<float> peakR        {0.0f};
};

/// Simple N-channel summing mixer.
/// Phase 1: Master bus only (sum of all active channels).
/// Phase 2+: Per-channel strips, pre/post fader sends, crossfader.
///
/// All methods called from the PortAudio callback thread — real-time safe.
class AudioMixer {
public:
    static constexpr int MAX_CHANNELS = 32;

    AudioMixer();
    ~AudioMixer() = default;

    /// Initialize mixer for given channel count, sample rate, buffer size.
    void initialize(int channels, double sampleRate, int framesPerBuffer);

    /// Mix a source buffer into the specified channel strip.
    /// channel: 0 = Deck A, 1 = Deck B, 2 = Encoder loopback, etc.
    void mixIn(int channel, const AudioBuffer& src);

    /// Render master mix to output buffer and clear channel accumulators.
    void renderMaster(AudioBuffer& out);

    /// Set channel gain (0.0–2.0, 1.0 = unity, RT-safe via atomic)
    void setChannelGain(int channel, float gain);
    void setChannelMute(int channel, bool muted);

    /// Master output gain
    void setMasterGain(float gain);

    /// Peak levels per channel (read from Qt thread for meters)
    float channelPeakL(int channel) const;
    float channelPeakR(int channel) const;
    float masterPeakL() const;
    float masterPeakR() const;

private:
    int    m_channels       = 2;
    double m_sampleRate     = 48000.0;
    int    m_framesPerBuffer= 512;

    // Per-channel accumulator buffers (MAX_CHANNELS x framesPerBuffer x 2)
    std::vector<float> m_channelBufs[MAX_CHANNELS];
    ChannelStrip       m_strips[MAX_CHANNELS];

    std::atomic<float> m_masterGain   {1.0f};
    std::atomic<float> m_masterPeakL  {0.0f};
    std::atomic<float> m_masterPeakR  {0.0f};
};

} // namespace M1
