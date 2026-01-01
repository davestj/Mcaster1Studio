#pragma once
#include <cstddef>
#include <cstring>
#include <vector>

namespace M1 {

/// Interleaved float32 PCM audio buffer shared between modules and the audio engine.
/// Samples are in the range [-1.0f, +1.0f].
/// Layout for stereo: [L0, R0, L1, R1, ...]
struct AudioBuffer {
    float*   data       = nullptr;  ///< Pointer to interleaved PCM samples
    int      channels   = 2;        ///< Number of channels (1=mono, 2=stereo)
    int      frames     = 0;        ///< Number of sample frames (samples per channel)
    double   sampleRate = 48000.0;  ///< Sample rate in Hz
    bool     isValid    = false;    ///< True if data is valid and non-null

    /// Total number of floats in data buffer
    int totalSamples() const noexcept { return channels * frames; }

    /// Access sample at frame f, channel ch
    float& at(int f, int ch) noexcept { return data[f * channels + ch]; }
    const float& at(int f, int ch) const noexcept { return data[f * channels + ch]; }

    /// Zero-fill the buffer
    void silence() noexcept {
        if (data && frames > 0)
            std::memset(data, 0, static_cast<size_t>(totalSamples()) * sizeof(float));
    }
};

/// Owned audio buffer backed by a std::vector (safe for off-RT-thread use)
struct AudioBufferOwned {
    std::vector<float> samples;
    int    channels   = 2;
    int    frames     = 0;
    double sampleRate = 48000.0;

    void resize(int ch, int fr, double sr) {
        channels   = ch;
        frames     = fr;
        sampleRate = sr;
        samples.assign(static_cast<size_t>(ch * fr), 0.0f);
    }

    AudioBuffer view() noexcept {
        AudioBuffer b;
        b.data       = samples.data();
        b.channels   = channels;
        b.frames     = frames;
        b.sampleRate = sampleRate;
        b.isValid    = !samples.empty();
        return b;
    }
};

} // namespace M1
