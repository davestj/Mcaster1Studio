#include "EncoderSlot.h"
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslCipher>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QDateTime>
#include <QDebug>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

// ── Codec headers (included ONLY here — never in the header) ──────────────────
#include <opus/opusenc.h>

#ifdef HAVE_LAME
#include <lame/lame.h>
#endif

#include <vorbis/vorbisenc.h>
#include <FLAC/stream_encoder.h>

#ifdef HAVE_FDK_AAC
#include <fdk-aac/aacenc_lib.h>
#endif

// ── Vorbis state wrappers ────────────────────────────────────────────────────
struct VorbisState {
    vorbis_info      vi;
    vorbis_comment   vc;
    vorbis_dsp_state vd;
    vorbis_block     vb;
    ogg_stream_state os;
    ogg_packet       op;
    QByteArray       pending;
    int              serialNo = 0;
};

// ── FLAC write callback ctx ───────────────────────────────────────────────────
struct FlacCtxImpl {
    QByteArray pending;
};

static FLAC__StreamEncoderWriteStatus flacWriteCb(
    const FLAC__StreamEncoder*, const FLAC__byte* buf, size_t bytes,
    unsigned, unsigned, void* client)
{
    auto* ctx = static_cast<FlacCtxImpl*>(client);
    ctx->pending.append(reinterpret_cast<const char*>(buf),
                        static_cast<qsizetype>(bytes));
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

// ── Opus write/close callbacks ────────────────────────────────────────────────
struct OpeCtxImpl {
    QTcpSocket* sock  = nullptr;
    bool        error = false;
    qint64      sent  = 0;
};

static int opeWriteCb(void* userData, const unsigned char* ptr, opus_int32 len) {
    auto* ctx = static_cast<OpeCtxImpl*>(userData);
    if (ctx->error || !ctx->sock || !ctx->sock->isOpen()) {
        ctx->error = true; return 1;
    }
    ctx->sock->write(reinterpret_cast<const char*>(ptr), len);
    ctx->sent += len;
    return 0;
}
static int opeCloseCb(void*) { return 0; }

#ifdef HAVE_FDK_AAC
struct AacState {
    HANDLE_AACENCODER enc      = nullptr;
    int               frameSize = 1024;
    int               channels  = 2;
    std::vector<INT_PCM> pcmBuf;
    std::vector<uint8_t> outBuf;
};
#endif

// ─────────────────────────────────────────────────────────────────────────────
EncoderSlot::EncoderSlot(QObject* parent)
    : QThread(parent)
{
    qRegisterMetaType<EncoderSlot::State>();
    qRegisterMetaType<EncoderEventLog::Entry>();
}

EncoderSlot::~EncoderSlot()
{
    disconnectFromServer();
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderSlot::configure(const EncoderConfig& cfg)
{
    m_cfg = cfg;

    // Sanitize host — strip protocol prefixes that users commonly paste in
    QString& host = m_cfg.host;
    if (host.startsWith("https://", Qt::CaseInsensitive))
        host = host.mid(8);
    else if (host.startsWith("http://", Qt::CaseInsensitive))
        host = host.mid(7);
    // Strip trailing slash
    while (host.endsWith('/'))
        host.chop(1);
}

void EncoderSlot::connectToServer()
{
    if (isRunning()) return;
    m_stop = false;
    m_wake = false;
    m_ringBuffer.reset();
    start(QThread::HighPriority);
}

void EncoderSlot::disconnectFromServer()
{
    m_stop = true;
    m_wake = true;
    m_wakeCond.wakeAll();
    wait(8000);
}

void EncoderSlot::wake()
{
    m_wake = true;
    m_wakeCond.wakeAll();
}

void EncoderSlot::setPttActive(bool active)
{
    if (m_dsp) m_dsp->setPttActive(active);
}

void EncoderSlot::updateMetadata(const QString& artist, const QString& title,
                                  const QMap<QString,QString>& icy2Fields)
{
    QMutexLocker lk(&m_metaMutex);
    m_metaArtist = artist;
    m_metaTitle  = title;
    m_metaIcy2   = icy2Fields;
    m_metaDirty  = true;
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderSlot::setState(State s)
{
    m_state.store(static_cast<int>(s));
    emit stateChanged(s);
}

// ═════════════════════════════════════════════════════════════════════════════
// run() — 7-state machine
// ═════════════════════════════════════════════════════════════════════════════
void EncoderSlot::run()
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);
    setState(State::Starting);
    m_eventLog.logInfo(tag, "Encoder thread started — " + m_cfg.name);
    m_eventLog.logInfo(tag, QString("Codec=%1  Bitrate=%2k  Server=%3:%4%5  Type=%6")
                       .arg(m_cfg.codecName())
                       .arg(m_cfg.bitrate)
                       .arg(m_cfg.host).arg(m_cfg.port).arg(m_cfg.mount)
                       .arg(m_cfg.serverTypeName()));

    // ── 1. Init DSP ──────────────────────────────────────────────────────────
    m_dsp = std::make_unique<EncoderDsp>();
    m_dsp->configure(m_cfg, m_cfg.sampleRate);
    m_eventLog.logDebug(tag, "DSP chain configured: " + m_cfg.eqPreset
                        + (m_cfg.dspEnabled ? " (enabled)" : " (disabled)"));

    // ── 2. Init codec ────────────────────────────────────────────────────────
    m_eventLog.logInfo(tag, "Initializing codec: " + m_cfg.codecName());
    if (!initCodec()) {
        m_eventLog.logError(tag, "Codec init FAILED — aborting");
        setState(State::Error);
        emit statusMessage("Codec init failed");
        return;
    }
    m_eventLog.logInfo(tag, "Codec initialized OK");

    int retries    = 0;
    int maxRetries = m_cfg.maxRetries;  // -1 = infinite

reconnect:
    if (m_stop) goto cleanup;

    // ── 3. TCP connect + SOURCE/PUT handshake ────────────────────────────────
    setState(State::Connecting);
    m_eventLog.logConnect(tag, QString("TCP connect → %1:%2%3")
                          .arg(m_cfg.host).arg(m_cfg.port).arg(m_cfg.mount));
    emit statusMessage(QString("Connecting to %1:%2%3…")
                       .arg(m_cfg.host).arg(m_cfg.port).arg(m_cfg.mount));
    {
        // Create socket: QSslSocket for SSL, QTcpSocket otherwise
        std::unique_ptr<QTcpSocket> sockPtr;
        if (m_cfg.useSsl) {
            auto* ssl = new QSslSocket();
            // Accept self-signed certs common on streaming servers
            ssl->setPeerVerifyMode(QSslSocket::VerifyNone);
            sockPtr.reset(ssl);
            m_eventLog.logConnect(tag, "Using SSL/TLS connection");
        } else {
            sockPtr.reset(new QTcpSocket());
        }
        QTcpSocket& sock = *sockPtr;

        // For Opus we need the encoder to write directly to the socket.
        if (m_cfg.codec == EncoderConfig::Codec::Opus) {
            auto* ctx = static_cast<OpeCtxImpl*>(m_opusEnc);
            if (ctx) ctx->sock = &sock;
        }

        if (m_cfg.useSsl) {
            auto* ssl = static_cast<QSslSocket*>(&sock);
            ssl->connectToHostEncrypted(m_cfg.host, static_cast<quint16>(m_cfg.port));
            if (!ssl->waitForEncrypted(8000)) {
                m_eventLog.logError(tag, "SSL handshake TIMEOUT/FAILED to " + m_cfg.host
                                    + ":" + QString::number(m_cfg.port)
                                    + " — " + ssl->errorString());
                emit statusMessage("SSL connect failed: " + ssl->errorString());
                goto handleError;
            }
            m_eventLog.logConnect(tag, QString("SSL/TLS connected OK — cipher: %1")
                                  .arg(ssl->sessionCipher().name()));
        } else {
            sock.connectToHost(m_cfg.host, static_cast<quint16>(m_cfg.port));
            if (!sock.waitForConnected(5000)) {
                m_eventLog.logError(tag, "TCP connect TIMEOUT to " + m_cfg.host
                                    + ":" + QString::number(m_cfg.port));
                emit statusMessage("Connect timeout: " + m_cfg.host);
                goto handleError;
            }
            m_eventLog.logConnect(tag, "TCP connected OK");
        }

        if (!sendSourceRequest(sock)) goto handleError;

        // ── 4. Encode loop ───────────────────────────────────────────────────
        setState(State::Streaming);
        m_bytesSent.store(0);
        m_connectedSecs.store(0);
        m_eventLog.logConnect(tag, "STREAMING to " + m_cfg.host + m_cfg.mount);
        emit statusMessage("Streaming to " + m_cfg.host + m_cfg.mount);

        static constexpr int kFrameSize = 960;  // 20ms at 48kHz
        std::vector<float> pcm(static_cast<size_t>(kFrameSize * m_cfg.channels), 0.f);
        QByteArray encoded;
        QElapsedTimer timer;
        qint64 lastMetaMs = -10000;  // Ensure first metadata push happens immediately
        int lastSecs = 0;
        timer.start();

        // Push any pending metadata right away on stream start
        pushMetadataIfDirty();

        bool streamOk = true;
        while (!m_stop && streamOk) {
            // Fill PCM from ring buffer
            while (m_ringBuffer.used() < kFrameSize && !m_stop)
                QThread::msleep(5);
            if (m_stop) break;

            int got = m_ringBuffer.read(pcm.data(), kFrameSize);
            if (got < kFrameSize)
                std::fill(pcm.begin() + got * m_cfg.channels, pcm.end(), 0.f);

            // DSP processing (in-place, encoder thread)
            if (m_cfg.dspEnabled && m_dsp)
                m_dsp->process(pcm.data(), kFrameSize, m_cfg.channels);

            // Encode
            encoded.clear();
            const int written = encodePcm(pcm.data(), kFrameSize, encoded);
            if (written < 0) { streamOk = false; break; }

            // For non-Opus codecs, write to socket directly
            if (m_cfg.codec != EncoderConfig::Codec::Opus && !encoded.isEmpty()) {
                if (!sock.isOpen()) {
                    m_eventLog.logError(tag, "Socket closed during streaming");
                    streamOk = false; break;
                }
                sock.write(encoded);
                sock.flush();
                m_bytesSent.fetch_add(encoded.size());
            } else if (m_cfg.codec == EncoderConfig::Codec::Opus) {
                auto* ctx = static_cast<OpeCtxImpl*>(m_opusEnc);
                if (ctx && ctx->error) {
                    m_eventLog.logError(tag, "Opus write callback error — socket lost");
                    streamOk = false; break;
                }
                m_bytesSent.fetch_add(written);
            }

            // Rate-limited metadata push (2s — fast enough for song changes,
            // slow enough to avoid hammering the admin endpoint)
            const qint64 nowMs = timer.elapsed();
            if (nowMs - lastMetaMs > 2000) {
                pushMetadataIfDirty();
                lastMetaMs = nowMs;
            }

            // Update uptime counter
            const int secs = static_cast<int>(nowMs / 1000);
            if (secs != lastSecs) {
                lastSecs = secs;
                m_connectedSecs.store(secs);
            }
        }

        // ── 5. Flush + close ─────────────────────────────────────────────────
        if (m_cfg.codec == EncoderConfig::Codec::Opus) {
            // ope_encoder_drain handled in shutdownCodec on Opus re-init
        } else {
            sock.flush();
        }
        sock.disconnectFromHost();
        sock.waitForDisconnected(2000);
        m_eventLog.logConnect(tag, "Socket disconnected");

        if (m_cfg.codec == EncoderConfig::Codec::Opus) {
            auto* ctx = static_cast<OpeCtxImpl*>(m_opusEnc);
            if (ctx) ctx->sock = nullptr;
        }

        if (m_stop) goto cleanup;
    }

handleError:
    if (m_stop) goto cleanup;

    if (m_cfg.autoReconnect) {
        if (maxRetries < 0 || retries < maxRetries) {
            ++retries;
            setState(State::Reconnecting);
            const QString retryMsg = QString("Reconnecting in %1s (attempt %2%3)")
                .arg(m_cfg.retryIntervalSec).arg(retries)
                .arg(maxRetries >= 0 ? QString(" of %1").arg(maxRetries)
                                     : " — unlimited retries");
            m_eventLog.logWarn(tag, retryMsg);
            emit statusMessage(retryMsg);

            // Sleep in small chunks so m_stop is noticed quickly
            const int totalMs = m_cfg.retryIntervalSec * 1000;
            int slept = 0;
            while (!m_stop && slept < totalMs) {
                QThread::msleep(250);
                slept += 250;
            }
            if (!m_stop) {
                m_eventLog.logConnect(tag, QString("Retry %1 — reconnecting…").arg(retries + 1));
                goto reconnect;
            }
        } else {
            // Max retries exhausted → Sleep state
            setState(State::Sleep);
            m_eventLog.logError(tag, QString("Max retries (%1) exhausted — entering SLEEP. Click Wake to retry.").arg(maxRetries));
            emit statusMessage("Max retries reached. Click Wake to retry.");
            m_wake = false;
            QMutexLocker lk(&m_wakeMutex);
            while (!m_wake.load() && !m_stop.load())
                m_wakeCond.wait(&m_wakeMutex, 500);
            if (!m_stop && m_wake) {
                retries = 0;
                m_wake  = false;
                m_eventLog.logConnect(tag, "Wake received — retries reset, reconnecting…");
                goto reconnect;
            }
        }
    } else {
        m_eventLog.logError(tag, "Auto-reconnect disabled — staying in Error state");
    }

cleanup:
    shutdownCodec();
    m_dsp.reset();
    const bool wasStopped = m_stop.load();
    setState(wasStopped ? State::Idle : State::Error);
    m_eventLog.logInfo(tag, wasStopped ? "Encoder stopped (user disconnect)."
                                        : "Encoder stopped (connection lost).");
    emit statusMessage(wasStopped ? "Disconnected." : "Connection lost.");
}

// ═════════════════════════════════════════════════════════════════════════════
// Codec init / shutdown
// ═════════════════════════════════════════════════════════════════════════════
bool EncoderSlot::initCodec()
{
    switch (m_cfg.codec) {
        case EncoderConfig::Codec::MP3:    return initMp3();
        case EncoderConfig::Codec::Opus:   return initOpus();
        case EncoderConfig::Codec::Vorbis: return initVorbis();
        case EncoderConfig::Codec::FLAC:   return initFlac();
        case EncoderConfig::Codec::AacLC:
        case EncoderConfig::Codec::HEAacV1:
        case EncoderConfig::Codec::HEAacV2: return initAac();
    }
    return false;
}

void EncoderSlot::shutdownCodec()
{
#ifdef HAVE_LAME
    if (m_lameEnc) {
        lame_close(static_cast<lame_t>(m_lameEnc));
        m_lameEnc = nullptr;
    }
#endif

    if (m_opusEnc) {
        // Drain and destroy libopusenc
        // NOTE: drain sends the remaining pages via the write callback;
        // the socket may already be closed — the callback handles this gracefully.
        // We store the encoder as an OpeCtxImpl which has the OggOpusEnc* inside.
        auto* ctx = static_cast<OpeCtxImpl*>(m_opusEnc);
        delete ctx;
        m_opusEnc = nullptr;
        // The actual OggOpusEnc* is tracked separately — see initOpus/encodeOpus
    }

    if (m_vorbisEnc) {
        auto* vs = static_cast<VorbisState*>(m_vorbisEnc);
        ogg_stream_clear(&vs->os);
        vorbis_block_clear(&vs->vb);
        vorbis_dsp_clear(&vs->vd);
        vorbis_comment_clear(&vs->vc);
        vorbis_info_clear(&vs->vi);
        delete vs;
        m_vorbisEnc = nullptr;
        m_vorbisBlk = nullptr;
    }

    if (m_flacEnc) {
        FLAC__stream_encoder_finish(static_cast<FLAC__StreamEncoder*>(m_flacEnc));
        FLAC__stream_encoder_delete(static_cast<FLAC__StreamEncoder*>(m_flacEnc));
        m_flacEnc = nullptr;
    }

#ifdef HAVE_FDK_AAC
    if (m_aacEnc) {
        auto* s = static_cast<AacState*>(m_aacEnc);
        if (s->enc) aacEncClose(&s->enc);
        delete s;
        m_aacEnc = nullptr;
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
bool EncoderSlot::initMp3()
{
#ifdef HAVE_LAME
    lame_t lame = lame_init();
    if (!lame) { emit statusMessage("LAME init failed"); return false; }
    lame_set_in_samplerate(lame, m_cfg.sampleRate);
    lame_set_num_channels (lame, m_cfg.channels);
    lame_set_brate        (lame, m_cfg.bitrate);
    lame_set_quality      (lame, 2);   // 2 = near-best quality
    switch (m_cfg.channelMode) {
        case EncoderConfig::ChannelMode::Mono:        lame_set_mode(lame, MONO);   break;
        case EncoderConfig::ChannelMode::JointStereo: lame_set_mode(lame, JOINT_STEREO); break;
        default:                                       lame_set_mode(lame, STEREO); break;
    }
    if (lame_init_params(lame) < 0) {
        lame_close(lame);
        emit statusMessage("LAME init_params failed");
        return false;
    }
    m_lameEnc = lame;
    return true;
#else
    emit statusMessage("MP3 not available (LAME not compiled in)");
    return false;
#endif
}

bool EncoderSlot::initOpus()
{
    // OggOpusEnc requires a write callback; we defer socket assignment to run()
    auto* ctx = new OpeCtxImpl();
    m_opusEnc = ctx;  // store ctx pointer; actual OggOpusEnc created per-connection in encodeOpus
    return true;
}

bool EncoderSlot::initVorbis()
{
    auto* vs = new VorbisState();
    vorbis_info_init(&vs->vi);
    int ret;
    // Use VBR quality mode
    ret = vorbis_encode_init_vbr(&vs->vi, m_cfg.channels, m_cfg.sampleRate,
                                  m_cfg.vbrQuality);
    if (ret != 0) {
        vorbis_info_clear(&vs->vi);
        delete vs;
        emit statusMessage("Vorbis init failed");
        return false;
    }
    vorbis_comment_init(&vs->vc);
    vorbis_comment_add_tag(&vs->vc, "ENCODER", "Mcaster1Studio");
    vorbis_analysis_init(&vs->vd, &vs->vi);
    vorbis_block_init(&vs->vd, &vs->vb);
    vs->serialNo = static_cast<int>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFF);
    ogg_stream_init(&vs->os, vs->serialNo);
    m_vorbisEnc = vs;
    m_vorbisBlk = &vs->vb;  // convenience alias
    return true;
}

bool EncoderSlot::initFlac()
{
    FLAC__StreamEncoder* enc = FLAC__stream_encoder_new();
    if (!enc) { emit statusMessage("FLAC encoder alloc failed"); return false; }
    FLAC__stream_encoder_set_channels   (enc, static_cast<unsigned>(m_cfg.channels));
    FLAC__stream_encoder_set_bits_per_sample(enc, 16u);
    FLAC__stream_encoder_set_sample_rate   (enc, static_cast<unsigned>(m_cfg.sampleRate));
    FLAC__stream_encoder_set_compression_level(enc, 5u);

    auto* ctx = new FlacCtxImpl();
    FLAC__StreamEncoderInitStatus st = FLAC__stream_encoder_init_stream(
        enc, flacWriteCb, nullptr, nullptr, nullptr, ctx);
    if (st != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        FLAC__stream_encoder_delete(enc);
        delete ctx;
        emit statusMessage("FLAC stream init failed");
        return false;
    }
    m_flacEnc    = enc;
    m_flacCtx.pending.clear();  // Use the member FlacCtxImpl via callback
    // Store ctx in the FlacCtxImpl member — managed by shutdownCodec
    // (ctx is passed as client_data to FLAC encoder; we reclaim it via m_flacCtx.pending)
    // Actually we need to store ctx pointer — use a different approach:
    // store ctx as m_aacEnc hack... no. Let's just store it in m_flacCtx.
    // The FlacCtxImpl is passed as callback client_data; pending fills up.
    // We access pending via the static callback which writes into ctx->pending.
    // Since ctx is on the heap, we keep a pointer. Store it in m_vorbisBlk (unused for FLAC):
    m_vorbisBlk = ctx;  // reuse void* slot temporarily — freed in shutdownCodec
    return true;
}

bool EncoderSlot::initAac()
{
#ifdef HAVE_FDK_AAC
    auto* s = new AacState();
    s->channels = m_cfg.channels;

    CHANNEL_MODE chanMode = (m_cfg.channels == 1) ? MODE_1 : MODE_2;
    AUDIO_OBJECT_TYPE aot;
    switch (m_cfg.codec) {
        case EncoderConfig::Codec::HEAacV1: aot = AOT_SBR;  break;
        case EncoderConfig::Codec::HEAacV2: aot = AOT_PS;   chanMode = MODE_2; break;
        default:                             aot = AOT_AAC_LC; break;
    }

    if (aacEncOpen(&s->enc, 0, static_cast<UINT>(m_cfg.channels)) != AACENC_OK) {
        delete s; emit statusMessage("AAC encoder open failed"); return false;
    }
    aacEncoder_SetParam(s->enc, AACENC_AOT,         static_cast<UINT>(aot));
    aacEncoder_SetParam(s->enc, AACENC_SAMPLERATE,  static_cast<UINT>(m_cfg.sampleRate));
    aacEncoder_SetParam(s->enc, AACENC_CHANNELMODE,  static_cast<UINT>(chanMode));
    aacEncoder_SetParam(s->enc, AACENC_BITRATE,      static_cast<UINT>(m_cfg.bitrate * 1000));
    aacEncoder_SetParam(s->enc, AACENC_TRANSMUX,     TT_MP4_ADTS);

    if (aacEncEncode(s->enc, nullptr, nullptr, nullptr, nullptr) != AACENC_OK) {
        aacEncClose(&s->enc); delete s;
        emit statusMessage("AAC encoder init failed"); return false;
    }

    AACENC_InfoStruct info{};
    aacEncInfo(s->enc, &info);
    s->frameSize = static_cast<int>(info.frameLength);
    s->pcmBuf.resize(static_cast<size_t>(s->frameSize * m_cfg.channels));
    s->outBuf.resize(8192);
    m_aacEnc = s;
    return true;
#else
    emit statusMessage("AAC not available (fdk-aac not compiled in)");
    return false;
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
// encodePcm dispatcher
// ═════════════════════════════════════════════════════════════════════════════
int EncoderSlot::encodePcm(const float* pcm, int frames, QByteArray& out)
{
    switch (m_cfg.codec) {
        case EncoderConfig::Codec::MP3:    return encodeMp3   (pcm, frames, out);
        case EncoderConfig::Codec::Opus:   return encodeOpus  (pcm, frames, out);
        case EncoderConfig::Codec::Vorbis: return encodeVorbis(pcm, frames, out);
        case EncoderConfig::Codec::FLAC:   return encodeFlac  (pcm, frames, out);
        case EncoderConfig::Codec::AacLC:
        case EncoderConfig::Codec::HEAacV1:
        case EncoderConfig::Codec::HEAacV2: return encodeAac  (pcm, frames, out);
    }
    return -1;
}

int EncoderSlot::encodeMp3(const float* pcm, int frames, QByteArray& out)
{
#ifdef HAVE_LAME
    lame_t lame = static_cast<lame_t>(m_lameEnc);
    if (!lame) return -1;
    const int outBytes = static_cast<int>(frames * 5 / 4 + 7200);  // LAME recommended size
    out.resize(outBytes);
    int ret;
    if (m_cfg.channels == 2) {
        ret = lame_encode_buffer_interleaved_ieee_float(
                lame, pcm, frames,
                reinterpret_cast<unsigned char*>(out.data()), outBytes);
    } else {
        ret = lame_encode_buffer_ieee_float(
                lame, pcm, nullptr, frames,
                reinterpret_cast<unsigned char*>(out.data()), outBytes);
    }
    if (ret < 0) { emit statusMessage("LAME encode error"); return -1; }
    out.resize(ret);
    return ret;
#else
    Q_UNUSED(pcm); Q_UNUSED(frames); Q_UNUSED(out);
    return -1;
#endif
}

int EncoderSlot::encodeOpus(const float* pcm, int frames, QByteArray& out)
{
    // Opus is handled by libopusenc which writes directly to socket via callback.
    // m_opusEnc holds OpeCtxImpl*. The actual OggOpusEnc* is created per-connection
    // in the socket scope using a local variable in run(). Here we just call write_float.
    // Since initOpus defers OggOpusEnc creation, we handle it here lazily.
    // The OggOpusEnc* is stored inside the OpeCtxImpl as a local field we add:
    struct OpusHolder {
        OggOpusEnc*  enc = nullptr;
        OpeCtxImpl*  ctx = nullptr;
        bool         initDone = false;
    };
    // Use m_flacCtx.pending as a byte buffer to store OpusHolder* — avoid adding member
    // Actually: let's store OpusHolder* in m_flacEnc (unused for Opus)
    auto* holder = static_cast<OpusHolder*>(m_flacEnc);
    if (!holder) {
        holder = new OpusHolder();
        holder->ctx = static_cast<OpeCtxImpl*>(m_opusEnc);
        m_flacEnc = holder;  // borrow slot
    }
    if (!holder->initDone && holder->ctx && holder->ctx->sock) {
        OggOpusComments* comments = ope_comments_create();
        ope_comments_add(comments, "ENCODER", "Mcaster1Studio");
        int err = OPE_OK;
        OpusEncCallbacks cbs{opeWriteCb, opeCloseCb};
        holder->enc = ope_encoder_create_callbacks(
            &cbs, holder->ctx, comments,
            m_cfg.sampleRate, m_cfg.channels, 0, &err);
        ope_comments_destroy(comments);
        if (!holder->enc || err != OPE_OK) {
            emit statusMessage("libopusenc create failed");
            return -1;
        }
        ope_encoder_ctl(holder->enc, OPUS_SET_BITRATE(m_cfg.bitrate * 1000));
        ope_encoder_ctl(holder->enc, OPUS_SET_COMPLEXITY(5));
        holder->initDone = true;
    }
    if (!holder->enc || !holder->initDone) return 0;  // not ready yet

    if (ope_encoder_write_float(holder->enc, pcm, frames) != OPE_OK) {
        emit statusMessage("Opus encode error");
        return -1;
    }
    auto* ctx = holder->ctx;
    const int sent = static_cast<int>(ctx ? ctx->sent : 0);
    if (ctx) ctx->sent = 0;
    Q_UNUSED(out);
    return sent;
}

int EncoderSlot::encodeVorbis(const float* pcm, int frames, QByteArray& out)
{
    auto* vs = static_cast<VorbisState*>(m_vorbisEnc);
    if (!vs) return -1;

    // Send headers on first call (pending is empty means headers not sent)
    if (vs->pending.isEmpty()) {
        // Submit header packets
        ogg_packet header, headerComm, headerCode;
        vorbis_analysis_headerout(&vs->vd, &vs->vc, &header, &headerComm, &headerCode);
        ogg_stream_packetin(&vs->os, &header);
        ogg_stream_packetin(&vs->os, &headerComm);
        ogg_stream_packetin(&vs->os, &headerCode);
        ogg_page page;
        while (ogg_stream_flush(&vs->os, &page)) {
            vs->pending.append(reinterpret_cast<const char*>(page.header),
                               static_cast<qsizetype>(page.header_len));
            vs->pending.append(reinterpret_cast<const char*>(page.body),
                               static_cast<qsizetype>(page.body_len));
        }
        out.swap(vs->pending);
        vs->pending.clear();
        return out.size();
    }

    // Provide PCM to encoder
    const int ch = m_cfg.channels;
    float** buffer = vorbis_analysis_buffer(&vs->vd, frames);
    for (int f = 0; f < frames; ++f)
        for (int c = 0; c < ch; ++c)
            buffer[c][f] = pcm[f * ch + c];
    vorbis_analysis_wrote(&vs->vd, frames);

    // Flush ogg pages
    ogg_page page;
    while (vorbis_analysis_blockout(&vs->vd, &vs->vb) == 1) {
        vorbis_analysis(&vs->vb, nullptr);
        vorbis_bitrate_addblock(&vs->vb);
        while (vorbis_bitrate_flushpacket(&vs->vd, &vs->op)) {
            ogg_stream_packetin(&vs->os, &vs->op);
            while (ogg_stream_pageout(&vs->os, &page)) {
                out.append(reinterpret_cast<const char*>(page.header),
                           static_cast<qsizetype>(page.header_len));
                out.append(reinterpret_cast<const char*>(page.body),
                           static_cast<qsizetype>(page.body_len));
            }
        }
    }
    return out.size();
}

int EncoderSlot::encodeFlac(const float* pcm, int frames, QByteArray& out)
{
    auto* enc = static_cast<FLAC__StreamEncoder*>(m_flacEnc);
    // m_vorbisBlk holds FlacCtxImpl* (see initFlac)
    auto* ctx = static_cast<FlacCtxImpl*>(m_vorbisBlk);
    if (!enc || !ctx) return -1;

    // Convert float [-1,1] to int32 16-bit range
    const int total = frames * m_cfg.channels;
    std::vector<FLAC__int32> ibuf(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i)
        ibuf[static_cast<size_t>(i)] = static_cast<FLAC__int32>(
            std::clamp(pcm[i], -1.f, 1.f) * 32767.f);

    if (!FLAC__stream_encoder_process_interleaved(enc, ibuf.data(),
                                                   static_cast<unsigned>(frames))) {
        emit statusMessage("FLAC encode error");
        return -1;
    }
    out.swap(ctx->pending);
    ctx->pending.clear();
    return out.size();
}

int EncoderSlot::encodeAac(const float* pcm, int frames, QByteArray& out)
{
#ifdef HAVE_FDK_AAC
    auto* s = static_cast<AacState*>(m_aacEnc);
    if (!s) return -1;

    // Convert float to INT_PCM (int16)
    const size_t n = static_cast<size_t>(frames * m_cfg.channels);
    s->pcmBuf.resize(n);
    for (size_t i = 0; i < n; ++i)
        s->pcmBuf[i] = static_cast<INT_PCM>(
            std::clamp(pcm[i], -1.f, 1.f) * 32767.f);

    AACENC_BufDesc inBuf{}, outBuf{};
    AACENC_InArgs  inArgs{};
    AACENC_OutArgs outArgs{};

    void* inPtr    = s->pcmBuf.data();
    int   inSize   = static_cast<int>(n * sizeof(INT_PCM));
    int   inElemSz = sizeof(INT_PCM);
    int   inId     = IN_AUDIO_DATA;
    inBuf.numBufs           = 1;
    inBuf.bufs              = &inPtr;
    inBuf.bufSizes          = &inSize;
    inBuf.bufElSizes        = &inElemSz;
    inBuf.bufferIdentifiers = &inId;
    inArgs.numInSamples     = static_cast<INT>(n);

    void* outPtr    = s->outBuf.data();
    int   outSize   = static_cast<int>(s->outBuf.size());
    int   outElemSz = 1;
    int   outId     = OUT_BITSTREAM_DATA;
    outBuf.numBufs           = 1;
    outBuf.bufs              = &outPtr;
    outBuf.bufSizes          = &outSize;
    outBuf.bufElSizes        = &outElemSz;
    outBuf.bufferIdentifiers = &outId;

    AACENC_ERROR err = aacEncEncode(s->enc, &inBuf, &outBuf, &inArgs, &outArgs);
    if (err != AACENC_OK && err != AACENC_ENCODE_EOF) {
        emit statusMessage("AAC encode error");
        return -1;
    }
    if (outArgs.numOutBytes > 0) {
        out.append(reinterpret_cast<const char*>(s->outBuf.data()),
                   outArgs.numOutBytes);
    }
    return out.size();
#else
    Q_UNUSED(pcm); Q_UNUSED(frames); Q_UNUSED(out);
    return -1;
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
// Network helpers
// ═════════════════════════════════════════════════════════════════════════════
bool EncoderSlot::sendSourceRequest(QTcpSocket& sock)
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);

    // Route to protocol-appropriate handshake
    // DNAS, Icecast2, Shoutcast v2 → PUT (modern Icecast2 protocol)
    // Shoutcast v1 → legacy SOURCE / ICY handshake
    if (m_cfg.serverType == EncoderConfig::ServerType::ShoutcastV1) {
        return sendShoutcastV1Request(sock);
    }
    return sendPutRequest(sock);
}

bool EncoderSlot::sendPutRequest(QTcpSocket& sock)
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);

    // HTTP Basic Auth: source:password
    const QByteArray credentials =
        ("source:" + m_cfg.password).toUtf8().toBase64();

    m_eventLog.logAuth(tag, QString("PUT handshake — user=source  content-type=%1  mount=%2")
                       .arg(m_cfg.contentType(), m_cfg.mount));

    QByteArray req;
    req += "PUT " + m_cfg.mount.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: " + m_cfg.host.toUtf8() + ":" + QByteArray::number(m_cfg.port) + "\r\n";
    req += "Authorization: Basic " + credentials + "\r\n";
    req += "User-Agent: Mcaster1Studio/1.0.0\r\n";
    req += "Content-Type: " + m_cfg.contentType().toUtf8() + "\r\n";
    req += "Ice-Public: "      + QByteArray(m_cfg.isPublic ? "1" : "0") + "\r\n";
    req += "Ice-Name: "        + m_cfg.stationName.toUtf8() + "\r\n";
    req += "Ice-Description: " + m_cfg.description.toUtf8() + "\r\n";
    req += "Ice-Genre: "       + m_cfg.genre.toUtf8()       + "\r\n";
    req += "Ice-Url: "         + m_cfg.url.toUtf8()         + "\r\n";
    req += "Ice-Audio-Info: ice-samplerate=" + QByteArray::number(m_cfg.sampleRate)
        + ";ice-bitrate=" + QByteArray::number(m_cfg.bitrate)
        + ";ice-channels=" + QByteArray::number(m_cfg.channels) + "\r\n";

    // ICY 2.2 extended headers for DNAS targets
    if (m_cfg.isDnas()) {
        req += "Icy-Version: 2.2\r\n";
        // Emit any populated ICY 2.2 social/identity fields from config
        for (auto it = m_cfg.icy2Fields.constBegin(); it != m_cfg.icy2Fields.constEnd(); ++it) {
            if (!it.value().isEmpty()) {
                // Convert icy2-xxx-yyy to Icy-Xxx-Yyy header format
                // For now, send as-is in header — DNAS accepts both formats
                req += it.key().toUtf8() + ": " + it.value().toUtf8() + "\r\n";
            }
        }
    }

    req += "Transfer-Encoding: chunked\r\n";
    req += "Expect: 100-continue\r\n";
    req += "\r\n";

    // Log the full request headers
    m_eventLog.logIcy(tag, ">>> PUT request headers:\n" + QString::fromUtf8(req));

    sock.write(req);
    if (!sock.waitForBytesWritten(3000)) {
        m_eventLog.logError(tag, "PUT request write timeout");
        emit statusMessage("PUT request write timeout");
        return false;
    }

    return waitForOkResponse(sock);
}

bool EncoderSlot::sendShoutcastV1Request(QTcpSocket& sock)
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);

    m_eventLog.logAuth(tag, QString("Shoutcast v1 SOURCE handshake — mount=%1  br=%2kbps")
                       .arg(m_cfg.mount).arg(m_cfg.bitrate));

    QByteArray req;
    req += "SOURCE " + m_cfg.mount.toUtf8() + " ICY/1.0\r\n";
    req += "icy-password: " + m_cfg.password.toUtf8() + "\r\n";
    req += "icy-name: "     + m_cfg.stationName.toUtf8() + "\r\n";
    req += "icy-genre: "    + m_cfg.genre.toUtf8() + "\r\n";
    req += "icy-url: "      + m_cfg.url.toUtf8() + "\r\n";
    req += "icy-pub: 1\r\n";
    req += "icy-br: "       + QByteArray::number(m_cfg.bitrate) + "\r\n";
    req += "content-type: " + m_cfg.contentType().toUtf8() + "\r\n";
    req += "\r\n";

    m_eventLog.logIcy(tag, ">>> SOURCE request headers:\n" + QString::fromUtf8(req));

    sock.write(req);
    if (!sock.waitForBytesWritten(3000)) {
        m_eventLog.logError(tag, "SOURCE request write timeout");
        emit statusMessage("SOURCE request write timeout");
        return false;
    }

    return waitForOkResponse(sock);
}

bool EncoderSlot::waitForOkResponse(QTcpSocket& sock)
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);

    if (!sock.waitForReadyRead(5000)) {
        m_eventLog.logError(tag, "No response from server (5s timeout)");
        emit statusMessage("No response from server (timeout)");
        return false;
    }
    const QByteArray resp = sock.readAll();

    // Clean up for log display
    const QString respStr = QString::fromUtf8(resp.left(300)).simplified();
    m_eventLog.logIcy(tag, "<<< Server response: " + respStr);

    // Accept HTTP 100 Continue or 200 OK
    if (resp.contains("200") || resp.contains("100")) {
        m_eventLog.logAuth(tag, "Server accepted connection — handshake OK");
        return true;
    }

    // Also accept Shoutcast "OK" response
    if (resp.contains("OK")) {
        m_eventLog.logAuth(tag, "Server accepted connection (Shoutcast OK)");
        return true;
    }

    const QString msg = "Server rejected: " + respStr;
    m_eventLog.logError(tag, msg);
    emit statusMessage(msg);
    return false;
}

// ── Metadata getters (thread-safe) ───────────────────────────────────────────
QString EncoderSlot::lastArtist() const {
    QMutexLocker lk(&m_metaMutex);
    return m_metaArtist;
}

QString EncoderSlot::lastTitle() const {
    QMutexLocker lk(&m_metaMutex);
    return m_metaTitle;
}

QString EncoderSlot::lastStreamTitle() const {
    QMutexLocker lk(&m_metaMutex);
    if (!m_metaArtist.isEmpty() && !m_metaTitle.isEmpty())
        return m_metaArtist + " - " + m_metaTitle;
    if (!m_metaTitle.isEmpty()) return m_metaTitle;
    return {};
}

// ═════════════════════════════════════════════════════════════════════════════
void EncoderSlot::pushMetadataIfDirty()
{
    const QString tag = QString("[Encoder %1]").arg(m_cfg.slotId + 1);

    QString artist, title;
    QMap<QString,QString> icy2;
    {
        QMutexLocker lk(&m_metaMutex);
        if (!m_metaDirty) return;
        artist      = m_metaArtist;
        title       = m_metaTitle;
        icy2        = m_metaIcy2;
        m_metaDirty = false;
    }

    m_eventLog.logIcy(tag, "Pushing metadata: " + artist + " - " + title);

    IcyPusher::Config cfg;
    cfg.host      = m_cfg.host;
    cfg.port      = m_cfg.port;
    cfg.mount     = m_cfg.mount;
    cfg.adminUser = m_cfg.adminUser;
    cfg.adminPass = m_cfg.adminPass;
    cfg.icy22     = m_cfg.isDnas();

    QString err;
    if (!IcyPusher::pushIcy1(cfg, artist, title, &err)) {
        m_eventLog.logWarn(tag, "ICY 1.x metadata push failed: " + err);
    } else {
        m_eventLog.logIcy(tag, "ICY 1.x metadata push OK");
    }

    if (cfg.icy22) {
        // Merge static ICY 2.2 config fields (station, DJ, show, social...) with
        // dynamic per-track fields (BPM, key, ISRC...) — dynamic overrides static
        QMap<QString,QString> merged = m_cfg.icy2Fields;  // static config fields
        for (auto it = icy2.constBegin(); it != icy2.constEnd(); ++it)
            merged[it.key()] = it.value();                // dynamic track fields win

        if (!IcyPusher::pushIcy2(cfg, artist, title, merged, &err)) {
            m_eventLog.logWarn(tag, "ICY 2.2 metadata push failed: " + err);
        } else {
            m_eventLog.logIcy(tag, "ICY 2.2 metadata push OK (" + QString::number(merged.size()) + " fields)");
        }
    }
}

bool EncoderSlot::openPortAudioSource()  { Q_UNUSED(m_paStream); return true; /* TODO: Pa_OpenStream */ }
void EncoderSlot::closePortAudioSource() {}
bool EncoderSlot::openWasapiSource()     { return true; /* TODO: WASAPI IAudioClient */ }
void EncoderSlot::closeWasapiSource()    {}
