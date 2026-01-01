#pragma once
#include "AudioRingBuffer.h"
#include "EncoderConfig.h"
#include "EncoderDsp.h"
#include "EncoderEventLog.h"
#include "IcyPusher.h"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <atomic>
#include <memory>

class QTcpSocket;

/// EncoderSlot — one streaming encoder instance.
///
/// Each slot runs in its own QThread and implements a 7-state machine:
///   Idle → Starting → Connecting → Streaming ⟶ Reconnecting ⟶ [Sleep] → Idle
///
/// Audio input modes:
///   LiveDeckMix   — RT audio mix; the EncoderModule feeds the ring buffer via onAudioBlock().
///   PortAudioDevice — This thread opens a PortAudio input stream and feeds its own ring buffer.
///   WasapiLoopback  — This thread uses WASAPI loopback capture.
///
/// Codec support (guarded by compile-time defines):
///   MP3 (HAVE_LAME), Opus (always), Vorbis (Ogg), FLAC, AAC-LC/HE-AAC (HAVE_FDK_AAC)
///
/// Per-slot DSP (EncoderDsp) processes PCM immediately before encoding.
class EncoderSlot : public QThread {
    Q_OBJECT

public:
    enum class State {
        Idle,         ///< Not started
        Starting,     ///< Initialising codec + DSP
        Connecting,   ///< TCP connecting to server
        Streaming,    ///< Live — encoding + sending
        Reconnecting, ///< Connection lost; watchdog retrying
        Sleep,        ///< Max retries exhausted — needs wake()
        Error         ///< Codec/source init failed
    };

    explicit EncoderSlot(QObject* parent = nullptr);
    ~EncoderSlot() override;

    // ── Configuration ─────────────────────────────────────────────────────────
    void            configure(const EncoderConfig& cfg);
    const EncoderConfig& config() const { return m_cfg; }

    // ── Control ───────────────────────────────────────────────────────────────
    void connectToServer();       ///< Start thread (Idle → Starting)
    void disconnectFromServer();  ///< Signal clean stop + wait

    /// Wake from Sleep state → attempt reconnect (Sleep → Connecting).
    void wake();

    // ── DSP ───────────────────────────────────────────────────────────────────
    /// Forward PTT active state to the per-slot DSP duck filter.
    void setPttActive(bool active);

    // ── State ─────────────────────────────────────────────────────────────────
    State state() const { return static_cast<State>(m_state.load()); }

    // ── Metadata ──────────────────────────────────────────────────────────────
    /// Update now-playing metadata (thread-safe; pushed on next opportunity).
    void updateMetadata(const QString& artist, const QString& title,
                        const QMap<QString, QString>& icy2Fields = {});

    /// Last-pushed metadata (thread-safe reads via m_metaMutex).
    QString lastArtist() const;
    QString lastTitle()  const;
    QString lastStreamTitle() const;  ///< "artist - title" formatted

    // ── Ring buffer (RT thread writes here for LiveDeckMix mode) ─────────────
    AudioRingBuffer& ringBuffer() { return m_ringBuffer; }

    // ── Live stats ────────────────────────────────────────────────────────────
    qint64  bytesSent()     const { return m_bytesSent.load(); }
    int     connectedSecs() const { return m_connectedSecs.load(); }

    // ── Peak levels for VU meters (RT-safe, set by EncoderModule::onAudioBlock) ──
    float peakL() const { return m_peakL.load(std::memory_order_relaxed); }
    float peakR() const { return m_peakR.load(std::memory_order_relaxed); }
    void  setPeakLevels(float l, float r) {
        m_peakL.store(l, std::memory_order_relaxed);
        m_peakR.store(r, std::memory_order_relaxed);
    }
    void  incrementAudioBlockCount() { m_audioBlockCount.fetch_add(1, std::memory_order_relaxed); }
    int   audioBlockCount() const    { return m_audioBlockCount.load(std::memory_order_relaxed); }

    // ── Event log ─────────────────────────────────────────────────────────────
    EncoderEventLog* eventLog() { return &m_eventLog; }

signals:
    void stateChanged(EncoderSlot::State state);
    void statusMessage(const QString& msg);

protected:
    void run() override;

private:
    // ── State helpers ─────────────────────────────────────────────────────────
    void setState(State s);

    // ── Codec lifecycle (codec headers included ONLY in EncoderSlot.cpp) ──────
    bool initCodec();
    void shutdownCodec();

    bool initMp3();
    bool initOpus();
    bool initVorbis();
    bool initFlac();
    bool initAac();

    /// Encode one buffer of PCM (interleaved float32).
    /// @returns number of bytes written to out, or -1 on error.
    int encodePcm(const float* pcm, int frames, QByteArray& out);

    int encodeMp3   (const float* pcm, int frames, QByteArray& out);
    int encodeOpus  (const float* pcm, int frames, QByteArray& out);
    int encodeVorbis(const float* pcm, int frames, QByteArray& out);
    int encodeFlac  (const float* pcm, int frames, QByteArray& out);
    int encodeAac   (const float* pcm, int frames, QByteArray& out);

    // ── Network ───────────────────────────────────────────────────────────────
    bool sendSourceRequest(QTcpSocket& sock);
    bool sendPutRequest(QTcpSocket& sock);         // Icecast2 / DNAS / Shoutcast v2
    bool sendShoutcastV1Request(QTcpSocket& sock);  // Shoutcast v1 legacy
    bool waitForOkResponse(QTcpSocket& sock);
    void pushMetadataIfDirty();

    // ── Audio source helpers ──────────────────────────────────────────────────
    bool openPortAudioSource();
    void closePortAudioSource();
    bool openWasapiSource();
    void closeWasapiSource();

    // ── Member data ───────────────────────────────────────────────────────────
    EncoderConfig       m_cfg;
    AudioRingBuffer     m_ringBuffer{48000 * 4, 2};  // 4s stereo ring buffer

    std::atomic<int>    m_state{static_cast<int>(State::Idle)};
    std::atomic<bool>   m_stop{false};
    std::atomic<bool>   m_wake{false};
    QMutex              m_wakeMutex;
    QWaitCondition      m_wakeCond;

    std::atomic<qint64> m_bytesSent{0};
    std::atomic<int>    m_connectedSecs{0};

    // Peak levels — written by RT thread, read by UI timer
    std::atomic<float>  m_peakL{0.0f};
    std::atomic<float>  m_peakR{0.0f};
    std::atomic<int>    m_audioBlockCount{0};  // Debug: counts onAudioBlock calls

    // DSP
    std::unique_ptr<EncoderDsp> m_dsp;

    // Per-slot event log
    EncoderEventLog m_eventLog;

    // Opaque codec handles — codec headers NEVER included in this header
    void* m_lameEnc    = nullptr;  // lame_global_flags*
    void* m_vorbisEnc  = nullptr;  // vorbis_dsp_state*
    void* m_vorbisBlk  = nullptr;  // vorbis_block*
    void* m_opusEnc    = nullptr;  // OpeWriteCtx* (opaque wrapper)
    void* m_flacEnc    = nullptr;  // FLAC__StreamEncoder*
    void* m_aacEnc     = nullptr;  // AacState*

    // PortAudio handle (opaque)
    void* m_paStream   = nullptr;  // PaStream*

    // Metadata
    mutable QMutex          m_metaMutex;
    QString                 m_metaArtist;
    QString                 m_metaTitle;
    QMap<QString,QString>   m_metaIcy2;
    bool                    m_metaDirty{false};

    // For Opus write callback
    struct OpeWriteCtx {
        QTcpSocket* sock      = nullptr;
        bool        error     = false;
        qint64      bytesSent = 0;
    };
    OpeWriteCtx m_opeCtx;

    // For FLAC write callback
    struct FlacWriteCtx {
        QByteArray pending;
    };
    FlacWriteCtx m_flacCtx;
};

Q_DECLARE_METATYPE(EncoderSlot::State)
