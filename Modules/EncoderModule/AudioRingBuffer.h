#pragma once
#include <atomic>
#include <vector>
#include <cstring>
#include <algorithm>

/// Lock-free single-producer / single-consumer ring buffer for interleaved float PCM.
///
/// The RT audio thread (producer) calls write().
/// The encoder thread  (consumer) calls read().
/// No mutexes — safe via memory_order_release / acquire fence pair.
class AudioRingBuffer {
public:
    /// @param capacityFrames  Ring capacity rounded up to next power-of-2.
    /// @param channels        Interleaved channel count (default stereo).
    explicit AudioRingBuffer(int capacityFrames = 48000 * 4, int channels = 2)
        : m_channels(channels)
        , m_capacity(nextPow2(std::max(capacityFrames, 64)))
        , m_buffer(static_cast<size_t>(m_capacity) * channels, 0.f)
    {}

    /// Write `frameCount` interleaved frames from the RT thread.
    /// Returns false (drop) when fewer than frameCount slots are free.
    bool write(const float* data, int frameCount) noexcept {
        if (available() < frameCount) return false;  // drop rather than block RT

        const int w = m_write.load(std::memory_order_relaxed);
        for (int i = 0; i < frameCount; ++i) {
            const int off = ((w + i) & (m_capacity - 1)) * m_channels;
            for (int c = 0; c < m_channels; ++c)
                m_buffer[static_cast<size_t>(off + c)] = data[i * m_channels + c];
        }
        m_write.store(w + frameCount, std::memory_order_release);
        return true;
    }

    /// Read up to `maxFrames` from the encoder thread.
    /// Returns number of frames actually read.
    int read(float* out, int maxFrames) noexcept {
        const int avail  = used();
        const int frames = std::min(avail, maxFrames);
        if (frames == 0) return 0;

        const int r = m_read.load(std::memory_order_relaxed);
        for (int i = 0; i < frames; ++i) {
            const int off = ((r + i) & (m_capacity - 1)) * m_channels;
            for (int c = 0; c < m_channels; ++c)
                out[i * m_channels + c] = m_buffer[static_cast<size_t>(off + c)];
        }
        m_read.store(r + frames, std::memory_order_release);
        return frames;
    }

    /// Frames currently in the buffer (written but not yet read).
    int used() const noexcept {
        return m_write.load(std::memory_order_acquire)
             - m_read.load(std::memory_order_acquire);
    }

    /// Free slots available for writing.
    int available() const noexcept { return m_capacity - used(); }

    /// Reset both positions (call only when both threads are idle).
    void reset() noexcept {
        m_read.store(0,  std::memory_order_relaxed);
        m_write.store(0, std::memory_order_relaxed);
    }

    int channels()  const noexcept { return m_channels;  }
    int capacity()  const noexcept { return m_capacity;  }

private:
    static int nextPow2(int n) noexcept {
        int p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    const int          m_channels;
    const int          m_capacity;
    std::vector<float> m_buffer;
    std::atomic<int>   m_write{0};
    std::atomic<int>   m_read{0};
};
