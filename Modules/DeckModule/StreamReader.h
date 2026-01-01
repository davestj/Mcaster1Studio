#pragma once
#include <QThread>
#include <QUrl>
#include <QMap>
#include <atomic>
#include <vector>

namespace M1 {

/// StreamReader — background thread for HTTP audio stream playback.
///
/// Uses FFmpeg to open an HTTP stream URL, decode audio, resample to
/// float32 stereo @ 48 kHz, and write to a lock-free SPSC ring buffer.
/// Supports ICY 1.x + ICY 2.2 metadata via custom HTTP headers.
///
/// Usage:
///   auto* reader = new StreamReader(this);
///   reader->open(QUrl("http://server:port/mount"));
///   // In RT callback: reader->readPcm(buf, frames);
///   reader->close();
class StreamReader : public QThread {
    Q_OBJECT

public:
    explicit StreamReader(QObject* parent = nullptr);
    ~StreamReader() override;

    /// Start streaming from the given URL (HTTP/HTTPS).
    /// Automatically resolves M3U/PLS playlist URLs.
    void open(const QUrl& url);

    /// Request the stream to stop. Call wait() after.
    void close();

    bool isOpen() const { return m_running.load(std::memory_order_relaxed); }

    /// Read decoded PCM into interleaved stereo float32 buffer.
    /// Returns number of frames actually read. RT-safe (lock-free).
    int readPcm(float* out, int maxFrames) noexcept;

    /// How many decoded frames are buffered and ready to read.
    int bufferedFrames() const noexcept;

    int sampleRate() const { return 48000; }

signals:
    /// Emitted once the stream is connected and audio starts decoding.
    void connected(const QString& stationName, const QString& genre,
                   const QString& contentType);

    /// Emitted when ICY/stream metadata changes (StreamTitle, icy2-* fields, etc.)
    void metadataChanged(const QMap<QString, QString>& fields);

    /// Emitted on connection or decode error.
    void streamError(const QString& msg);

    /// Emitted when the stream ends or the reader shuts down.
    void disconnected();

protected:
    void run() override;

private:
    /// Resolve M3U/PLS playlist URL to the actual stream URL.
    QUrl resolvePlaylist(const QUrl& url);

    QUrl               m_url;
    std::atomic<bool>  m_running{false};

    // ── Lock-free SPSC ring buffer (stereo float32) ──────────────
    static constexpr int kRingFrames = 1 << 18;  // 262144 frames ≈ 5.5 s @ 48 kHz
    static constexpr int kRingMask   = kRingFrames - 1;
    std::vector<float> m_ring;                     // size = kRingFrames * 2
    std::atomic<int>   m_ringW{0};
    std::atomic<int>   m_ringR{0};

    void ringWrite(const float* stereoData, int frames);
};

} // namespace M1
