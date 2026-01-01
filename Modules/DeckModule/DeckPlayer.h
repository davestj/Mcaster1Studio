#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QAudioDecoder>
#include <atomic>
#include <vector>
#include <array>
#include <cstdint>

namespace M1 {

class StreamReader;

/// DeckPlayer — audio playback engine for one physical deck (A or B).
///
/// Responsibilities:
///   - Load an audio file via QAudioDecoder (asynchronous, Qt thread)
///   - Load an HTTP stream via StreamReader (FFmpeg + ICY 1.x/2.2)
///   - Store decoded PCM as float32 stereo interleaved
///   - Provide real-time-safe processBlock() for PortAudio callback
///   - Track playback position using 16.16 fixed-point arithmetic
///   - Support variable speed/pitch via linear interpolation
///   - Manage cue point, loop in/out, and 4 hot cues
///   - Compute waveform peak data for WaveformView
///   - Estimate BPM via autocorrelation on the onset envelope
///
/// Thread safety: processBlock() is called from the RT audio thread.
/// All RT-accessible state uses std::atomic<>. Qt slots are on the Qt thread.
class DeckPlayer : public QObject {
    Q_OBJECT

public:
    static constexpr int kHotCueCount = 4;

    enum class State { Empty, Loading, Ready, Playing, Paused };

    explicit DeckPlayer(int deckIndex, QObject* parent = nullptr);
    ~DeckPlayer() override;

    // ─── File / stream loading ───────────────────────────────────
    void loadFile(const QString& path);
    void loadUrl(const QUrl& url);
    void unload();

    QString  loadedPath() const { return m_path; }
    State    state()      const { return m_state; }
    bool     isStreaming() const { return m_isStreaming; }

    // ─── Transport (Qt thread) ────────────────────────────────────
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // ─── Position (Qt thread) ─────────────────────────────────────
    qint64 positionSamples()   const;  // current position in frames
    qint64 totalSamples()      const { return m_totalFrames.load(std::memory_order_relaxed); }
    double positionSeconds()   const;
    double durationSeconds()   const;

    void   seek(qint64 frame);

    // ─── Cue / loop / hot cues (Qt thread) ───────────────────────
    void setCuePoint();
    void jumpToCue();
    qint64 cuePoint() const { return m_cueFrame; }

    void setLoop(bool enabled, qint64 inFrame = -1, qint64 outFrame = -1);
    bool loopEnabled()  const { return m_loopEnabled; }
    qint64 loopIn()     const { return m_loopIn; }
    qint64 loopOut()    const { return m_loopOut; }

    void   setHotCue(int index, qint64 frame = -1); // -1 = use current position
    void   clearHotCue(int index);
    void   jumpToHotCue(int index);
    qint64 hotCue(int index) const;

    // ─── Speed / pitch ────────────────────────────────────────────
    void  setSpeed(float rate);    // 1.0 = normal, 1.5 = +50%, 0.5 = -50%
    float speed() const;
    float bpm()   const { return m_bpm; }

    // ─── Tag metadata (Qt thread, populated after loadFile) ───────
    QString tagTitle()  const { return m_tagTitle;  }
    QString tagArtist() const { return m_tagArtist; }
    QString tagAlbum()  const { return m_tagAlbum;  }
    QString tagGenre()  const { return m_tagGenre;  }
    QString tagYear()   const { return m_tagYear;   }
    int     bitrate()    const { return m_bitrate;    }  ///< kbps; 0 if uncompressed
    int     sampleRate() const { return m_sampleRate; }  ///< Hz (e.g. 44100, 48000)
    int     channels()   const { return m_channels;   }  ///< 1=mono, 2=stereo

    // ─── Per-deck output gain (volume fader) ─────────────────────
    void  setGain(float g);   ///< 0.0–1.0; RT-safe
    float gain()  const { return m_gain.load(std::memory_order_relaxed); }

    // ─── AIR / CUE routing ─────────────────────────────────────────
    void setAirOn(bool v) { m_airOn.store(v, std::memory_order_relaxed); }
    void setCueOn(bool v) { m_cueOn.store(v, std::memory_order_relaxed); }
    bool airOn() const { return m_airOn.load(std::memory_order_relaxed); }
    bool cueOn() const { return m_cueOn.load(std::memory_order_relaxed); }

    // ─── Per-deck 3-band EQ ───────────────────────────────────────
    /// band: 0=Low(100Hz shelf) 1=Mid(1kHz peak) 2=High(10kHz shelf); dB ±12; RT-safe
    void  setEqGain(int band, float dB);
    float eqGain(int band) const;

    // ─── Levels (updated each processBlock) ──────────────────────
    float levelL() const { return m_levelL.load(std::memory_order_relaxed); }
    float levelR() const { return m_levelR.load(std::memory_order_relaxed); }
    void  resetLevels();

    // ─── Waveform data ────────────────────────────────────────────
    /// Pre-computed peak per pixel column (stereo average). Used by WaveformView.
    const std::vector<float>& waveformPeaks() const { return m_peaks; }
    int                       waveformWidth() const { return (int)m_peaks.size(); }

    // ─── RT audio processing ─────────────────────────────────────
    /// Called from PortAudio RT thread. Adds this deck's audio into output.
    /// out: interleaved float32 buffer [L0,R0,L1,R1,...], frames×outChannels floats
    void processBlock(float* out, int frames, int outChannels, float gain);

signals:
    void stateChanged(DeckPlayer::State state);
    void loadingProgress(int percent);
    void loadingFinished();
    void loadingError(const QString& err);
    void positionChanged(qint64 frame);
    void hotCuesChanged();
    void bpmDetected(float bpm);
    void waveformReady();  ///< Emitted when background waveform analysis completes
    void finished();       ///< Emitted when playback reaches end of track
    void tagsLoaded();     ///< Emitted on Qt thread after tag metadata is read
    void streamMetadataChanged(const QMap<QString, QString>& metadata); ///< ICY metadata update

private slots:
    void onDecoderBufferReady();
    void onDecoderFinished();
    void onDecoderError(QAudioDecoder::Error error);
    void onEofPoll();      ///< Polls m_playing to detect RT-thread EOF

private:
    void  setState(State s);
    void  buildWaveform();
    float detectBpm();
    void  processStreamBlock(float* out, int frames, int outChannels, float gain);

    // Identity
    int     m_deckIndex;
    QString m_path;
    State   m_state = State::Empty;

    // Decoded audio (float32 stereo interleaved, native sample rate)
    std::vector<float> m_pcm;
    int    m_channels    = 2;
    int    m_sampleRate  = 48000;
    std::atomic<qint64> m_totalFrames{0};

    // Waveform summary (one peak value per display column)
    std::vector<float> m_peaks;
    static constexpr int kWaveformColumns = 4000; // pixels

    // BPM
    float m_bpm = 0.0f;

    // Tag metadata (Qt thread only)
    QString            m_tagTitle;
    QString            m_tagArtist;
    QString            m_tagAlbum;
    QString            m_tagGenre;
    QString            m_tagYear;
    int                m_bitrate    = 0;

    // Per-deck volume gain (RT-safe)
    std::atomic<float> m_gain{1.0f};

    // ─── AIR / CUE routing flags (RT-safe) ───────────────────────
    std::atomic<bool> m_airOn{true};    ///< Route to main/broadcast output
    std::atomic<bool> m_cueOn{false};   ///< Route to headphone/cue output

    // ─── RT-safe atomics ──────────────────────────────────────────
    // Position: 16.16 fixed-point (upper 48 bits = frame index, lower 16 = sub-frame)
    std::atomic<qint64> m_posFixed{0};
    std::atomic<bool>   m_playing{false};
    std::atomic<float>  m_speed{1.0f};
    std::atomic<float>  m_levelL{0.0f};
    std::atomic<float>  m_levelR{0.0f};

    // Loop (written on Qt thread, read on RT thread — uses acquire/release)
    std::atomic<bool>   m_loopEnabledAtomic{false};
    std::atomic<qint64> m_loopInAtomic{0};
    std::atomic<qint64> m_loopOutAtomic{-1};

    // Mirror on Qt thread for UI queries
    bool   m_loopEnabled = false;
    qint64 m_loopIn      = 0;
    qint64 m_loopOut     = -1;
    qint64 m_cueFrame    = 0;
    std::array<qint64, kHotCueCount> m_hotCues{-1, -1, -1, -1};

    // Decoder (Qt thread only)
    QAudioDecoder* m_decoder    = nullptr;
    QTimer*        m_eofTimer   = nullptr;  ///< Polls m_playing for RT-thread EOF detection
    int            m_decodeGen  = 0;        ///< Incremented on each loadFile/unload to ignore stale signals
    int            m_activeGen  = 0;        ///< The generation that the current active decode belongs to

    // Streaming (HTTP/ICY)
    StreamReader*  m_streamReader = nullptr;
    bool           m_isStreaming  = false;

    // ─── 3-band biquad EQ (Direct Form II Transposed) ────────────
    struct BqCoef  { double b0=1,b1=0,b2=0,a1=0,a2=0; };  // normalised (a0=1)
    struct BqState { double w1=0, w2=0; };                  // per-channel delay state

    static BqCoef  makeLowShelf (double freq, double fs, double gainDb);
    static BqCoef  makeHighShelf(double freq, double fs, double gainDb);
    static BqCoef  makePeakingEQ(double freq, double Q, double fs, double gainDb);
    static double  processBq(const BqCoef& c, BqState& s, double x) noexcept;

    std::atomic<float> m_eqGainDb[3]{ 0.0f, 0.0f, 0.0f };  // [Low, Mid, High]
    std::atomic<bool>  m_eqDirty [3]{ true, true, true };   // force initial coef compute
    BqCoef             m_eqCoef  [3];                        // RT-thread owned
    BqState            m_eqState [3][2];                     // [band][channel 0=L 1=R]
};

} // namespace M1

Q_DECLARE_METATYPE(M1::DeckPlayer::State)
