#pragma once
#include <QObject>
#include <QString>
#include <QAudioDecoder>
#include <QTimer>
#include <atomic>
#include <vector>
#include <cstdint>

namespace M1 {

/// CartPlayer — lightweight audio player for a single cart slot.
///
/// Similar to DeckPlayer but stripped to essentials: load file, decode to
/// float32 PCM buffer, instant play/stop, volume control. No EQ, no loop,
/// no cue points, no streaming.
///
/// Thread safety: processBlock() called from RT audio thread via CartWallModule.
/// Uses std::atomic for all shared state.
class CartPlayer : public QObject {
    Q_OBJECT

public:
    enum class State { Empty, Loading, Ready, Playing };

    explicit CartPlayer(QObject* parent = nullptr);
    ~CartPlayer() override;

    // ── File loading (Qt thread) ────────────────────────────────────
    void loadFile(const QString& path);
    void unload();

    QString   loadedPath() const { return m_path; }
    State     state()      const { return m_state; }
    bool      isPlaying()  const { return m_playing.load(std::memory_order_relaxed); }

    // ── Transport (Qt thread) ───────────────────────────────────────
    void play();
    void stop();

    // ── Volume (RT-safe) ────────────────────────────────────────────
    void  setGain(float g) { m_gain.store(g, std::memory_order_relaxed); }
    float gain()    const  { return m_gain.load(std::memory_order_relaxed); }

    // ── Duration info (Qt thread, after load) ───────────────────────
    double durationSeconds() const;
    int    sampleRate() const { return m_sampleRate; }

    // ── Levels (updated each processBlock) ──────────────────────────
    float peakLevel() const { return m_peak.load(std::memory_order_relaxed); }

    // ── RT audio processing ─────────────────────────────────────────
    /// Called from RT thread. Adds this cart's audio into output buffer.
    /// out: interleaved float32 [L0,R0,L1,R1,...], frames x outChannels
    void processBlock(float* out, int frames, int outChannels) noexcept;

signals:
    void stateChanged(CartPlayer::State s);
    void loadingFinished();
    void loadingError(const QString& err);
    void playbackFinished();  ///< Emitted when cart reaches end

private slots:
    void onDecoderBufferReady();
    void onDecoderFinished();
    void onDecoderError(QAudioDecoder::Error error);
    void onEofPoll();

private:
    void setState(State s);

    QString m_path;
    State   m_state = State::Empty;

    // Decoded PCM (float32 stereo interleaved)
    std::vector<float> m_pcm;
    int m_channels   = 2;
    int m_sampleRate = 48000;
    int m_totalFrames = 0;

    // RT-safe atomics
    std::atomic<int>   m_pos{0};       // current frame position
    std::atomic<bool>  m_playing{false};
    std::atomic<float> m_gain{1.0f};
    std::atomic<float> m_peak{0.0f};

    // Decoder (Qt thread only)
    QAudioDecoder* m_decoder  = nullptr;
    QTimer*        m_eofTimer = nullptr;
};

} // namespace M1

Q_DECLARE_METATYPE(M1::CartPlayer::State)
