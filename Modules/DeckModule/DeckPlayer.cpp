#include "DeckPlayer.h"
#include "StreamReader.h"
#include <QAudioFormat>
#include <QSettings>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDebug>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

namespace M1 {

static constexpr qint64 kSubFrameBits = 16;

// ─── 3-band biquad EQ — Audio EQ Cookbook formulas ────────────────────────────
DeckPlayer::BqCoef DeckPlayer::makeLowShelf(double f, double fs, double dB) {
    const double A  = std::pow(10.0, dB / 40.0);
    const double w0 = 2.0 * M_PI * f / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.707 - 1.0) + 2.0);
    const double sqA   = std::sqrt(A);
    const double a0 = (A+1) + (A-1)*cw + 2.0*sqA*alpha;
    return { A*((A+1)-(A-1)*cw+2.0*sqA*alpha)/a0,
             2.0*A*((A-1)-(A+1)*cw)/a0,
             A*((A+1)-(A-1)*cw-2.0*sqA*alpha)/a0,
             -2.0*((A-1)+(A+1)*cw)/a0,
             ((A+1)+(A-1)*cw-2.0*sqA*alpha)/a0 };
}
DeckPlayer::BqCoef DeckPlayer::makeHighShelf(double f, double fs, double dB) {
    const double A  = std::pow(10.0, dB / 40.0);
    const double w0 = 2.0 * M_PI * f / fs;
    const double cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / 2.0 * std::sqrt((A + 1.0/A) * (1.0/0.707 - 1.0) + 2.0);
    const double sqA   = std::sqrt(A);
    const double a0 = (A+1) - (A-1)*cw + 2.0*sqA*alpha;
    return { A*((A+1)+(A-1)*cw+2.0*sqA*alpha)/a0,
             -2.0*A*((A-1)+(A+1)*cw)/a0,
             A*((A+1)+(A-1)*cw-2.0*sqA*alpha)/a0,
             2.0*((A-1)-(A+1)*cw)/a0,
             ((A+1)-(A-1)*cw-2.0*sqA*alpha)/a0 };
}
DeckPlayer::BqCoef DeckPlayer::makePeakingEQ(double f, double Q, double fs, double dB) {
    const double A  = std::pow(10.0, dB / 40.0);
    const double w0 = 2.0 * M_PI * f / fs;
    const double alpha = std::sin(w0) / (2.0 * Q);
    const double a0 = 1.0 + alpha / A;
    return { (1.0 + alpha * A) / a0,
             (-2.0 * std::cos(w0)) / a0,
             (1.0 - alpha * A) / a0,
             (-2.0 * std::cos(w0)) / a0,
             (1.0 - alpha / A) / a0 };
}
double DeckPlayer::processBq(const BqCoef& c, BqState& s, double x) noexcept {
    const double y = c.b0 * x + s.w1;
    s.w1 = c.b1 * x - c.a1 * y + s.w2;
    s.w2 = c.b2 * x - c.a2 * y;
    return y;
}
void DeckPlayer::setEqGain(int band, float dB) {
    if (band < 0 || band > 2) return;
    m_eqGainDb[band].store(dB, std::memory_order_relaxed);
    m_eqDirty [band].store(true, std::memory_order_release);
}
float DeckPlayer::eqGain(int band) const {
    if (band < 0 || band > 2) return 0.0f;
    return m_eqGainDb[band].load(std::memory_order_relaxed);
}
static constexpr qint64 kSubFrameUnit = 1LL << kSubFrameBits; // 65536

DeckPlayer::DeckPlayer(int deckIndex, QObject* parent)
    : QObject(parent)
    , m_deckIndex(deckIndex)
{
    qRegisterMetaType<M1::DeckPlayer::State>();

    m_decoder = new QAudioDecoder(this);

    // Request float32 stereo output from the decoder at the engine's sample rate.
    // Hardcoding 48000 when the engine runs at 44100 causes ~8% slower playback.
    QSettings engineSettings("Mcaster1", "Mcaster1Studio");
    const int engineSR = engineSettings.value("audio/sampleRate", 48000).toInt();

    QAudioFormat fmt;
    fmt.setSampleRate(engineSR);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Float);
    m_decoder->setAudioFormat(fmt);

    connect(m_decoder, &QAudioDecoder::bufferReady, this, &DeckPlayer::onDecoderBufferReady);
    connect(m_decoder, &QAudioDecoder::finished,    this, &DeckPlayer::onDecoderFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
            this, &DeckPlayer::onDecoderError);

    // EOF detection: poll m_playing at 50ms; if Playing state but m_playing==false → track ended
    m_eofTimer = new QTimer(this);
    m_eofTimer->setInterval(50);
    connect(m_eofTimer, &QTimer::timeout, this, &DeckPlayer::onEofPoll);
    m_eofTimer->start();
}

DeckPlayer::~DeckPlayer() = default;

// ─── File loading ─────────────────────────────────────────────────────────────
void DeckPlayer::loadFile(const QString& path) {
    if (path.isEmpty()) return;

    // Detect HTTP(S) URLs — route to stream reader
    if (path.startsWith("http://", Qt::CaseInsensitive) ||
        path.startsWith("https://", Qt::CaseInsensitive)) {
        loadUrl(QUrl(path));
        return;
    }

    // Detect local M3U/PLS playlists containing a stream URL
    const QString lower = path.toLower();
    if (lower.endsWith(".m3u") || lower.endsWith(".m3u8") || lower.endsWith(".pls")) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!f.atEnd()) {
                QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (lower.endsWith(".pls") && line.startsWith("File1=", Qt::CaseInsensitive))
                    line = line.mid(6).trimmed();
                if (line.startsWith("http://", Qt::CaseInsensitive) ||
                    line.startsWith("https://", Qt::CaseInsensitive)) {
                    loadUrl(QUrl(line));
                    return;
                }
                if (!line.isEmpty() && !line.startsWith('#') && !lower.endsWith(".pls")) {
                    // Non-comment, non-empty line in M3U — could be a local file
                    break;
                }
            }
        }
    }

    unload();
    m_path = path;
    setState(State::Loading);

    // Read tag metadata synchronously (fast I/O, Qt thread)
    m_tagTitle.clear(); m_tagArtist.clear(); m_tagAlbum.clear();
    m_tagGenre.clear(); m_tagYear.clear(); m_bitrate = 0;
    {
        TagLib::FileRef f(path.toLocal8Bit().constData());
        if (!f.isNull()) {
            if (TagLib::Tag* tag = f.tag()) {
                m_tagTitle  = QString::fromStdString(tag->title().to8Bit(true)).trimmed();
                m_tagArtist = QString::fromStdString(tag->artist().to8Bit(true)).trimmed();
                m_tagAlbum  = QString::fromStdString(tag->album().to8Bit(true)).trimmed();
                m_tagGenre  = QString::fromStdString(tag->genre().to8Bit(true)).trimmed();
                if (tag->year() > 0)
                    m_tagYear = QString::number(tag->year());
            }
            if (TagLib::AudioProperties* ap = f.audioProperties())
                m_bitrate = ap->bitrate();
        }
    }
    emit tagsLoaded();

    ++m_decodeGen;
    m_activeGen = m_decodeGen;  // stamp this decode session
    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
    qInfo() << "[Deck" << m_deckIndex << "] Loading:" << path
            << "| Artist:" << m_tagArtist << "| Title:" << m_tagTitle;
}

// ─── Stream loading (HTTP/ICY) ────────────────────────────────────────────────
void DeckPlayer::loadUrl(const QUrl& url) {
    unload();
    m_path = url.toString();
    m_isStreaming = true;
    setState(State::Loading);

    m_streamReader = new StreamReader(this);

    connect(m_streamReader, &StreamReader::connected, this,
        [this](const QString& name, const QString& /*genre*/, const QString& /*ct*/) {
            m_tagTitle  = name;
            m_tagArtist.clear();
            m_tagAlbum  = "Live Stream";
            emit tagsLoaded();
            setState(State::Ready);
            play();  // auto-play streams
            emit loadingFinished();
        });

    connect(m_streamReader, &StreamReader::metadataChanged, this,
        [this](const QMap<QString, QString>& meta) {
            QString title = meta.value("StreamTitle", meta.value("title", ""));
            if (title.isEmpty()) return;

            // Parse "Artist - Title" format common in ICY
            int sep = title.indexOf(" - ");
            if (sep >= 0) {
                m_tagArtist = title.left(sep).trimmed();
                m_tagTitle  = title.mid(sep + 3).trimmed();
            } else {
                m_tagTitle = title;
            }
            emit tagsLoaded();
            emit streamMetadataChanged(meta);
        });

    connect(m_streamReader, &StreamReader::streamError, this,
        [this](const QString& err) {
            qWarning() << "[Deck" << m_deckIndex << "] Stream error:" << err;
            emit loadingError(err);
            m_isStreaming = false;
            setState(State::Empty);
        });

    connect(m_streamReader, &StreamReader::disconnected, this,
        [this]() {
            if (m_state == State::Playing) {
                m_playing.store(false, std::memory_order_release);
                setState(State::Ready);
                emit finished();
            }
        });

    m_streamReader->open(url);
    qInfo() << "[Deck" << m_deckIndex << "] Streaming:" << url.toString();
}

void DeckPlayer::unload() {
    // Invalidate any pending decoder signals from the old decode session.
    // This prevents stale finished/bufferReady signals (queued cross-thread)
    // from triggering loadingFinished after unload clears the PCM buffer.
    ++m_decodeGen;

    // Stop streaming if active
    if (m_streamReader) {
        m_streamReader->close();
        m_streamReader->wait(3000);
        delete m_streamReader;
        m_streamReader = nullptr;
    }
    m_isStreaming = false;

    m_decoder->stop();
    stop(); // sets m_playing = false (RT thread stops accessing m_pcm)
    // Zero totalFrames first so any in-flight RT call bails at the frame check
    m_totalFrames.store(0, std::memory_order_release);
    m_pcm.clear();
    m_peaks.clear();
    m_bpm = 0.0f;
    m_cueFrame = 0;
    m_hotCues.fill(-1);
    m_loopEnabled = false;
    m_loopEnabledAtomic.store(false);
    m_loopIn = 0; m_loopOut = -1;
    m_loopInAtomic.store(0); m_loopOutAtomic.store(-1);
    m_path.clear();
    setState(State::Empty);
}

void DeckPlayer::onDecoderBufferReady() {
    // Only accept buffers from the active decode session
    while (m_decoder->bufferAvailable()) {
        QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) continue;
        if (m_decodeGen != m_activeGen) return;  // stale buffer from previous decode

        const float* src = buf.constData<float>();
        const int nSamples = buf.sampleCount(); // = frameCount × channelCount
        m_pcm.insert(m_pcm.end(), src, src + nSamples);
        m_channels   = buf.format().channelCount();
        m_sampleRate = buf.format().sampleRate();

        // Emit approximate progress
        if (m_decoder->duration() > 0) {
            int pct = (int)((m_decoder->position() * 100) / m_decoder->duration());
            emit loadingProgress(pct);
        }
    }
}

void DeckPlayer::onDecoderFinished() {
    // Guard: ignore stale finished signals from a previous decode session.
    // When loadFile() is called while a decode is in progress, the old decoder's
    // finished signal may already be queued (cross-thread QueuedConnection).
    // Without this guard, the stale signal triggers loadingFinished → play()
    // while the NEW decode is still running, causing a data race on m_pcm.
    if (m_decodeGen != m_activeGen || m_state != State::Loading) {
        qInfo() << "[Deck" << m_deckIndex << "] Ignoring stale decoder finished signal"
                << "(gen:" << m_activeGen << "vs" << m_decodeGen
                << "state:" << (int)m_state << ")";
        return;
    }

    m_totalFrames.store((qint64)(m_pcm.size() / m_channels), std::memory_order_release);
    qInfo() << "[Deck" << m_deckIndex << "] Loaded:"
            << m_totalFrames.load() << "frames @" << m_sampleRate << "Hz";

    // Make the deck immediately playable — don't block the UI thread
    setState(State::Ready);
    emit loadingFinished();

    // Waveform + BPM analysis runs in background; results emitted when done
    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        emit waveformReady();
        if (m_bpm > 0)
            emit bpmDetected(m_bpm);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([this]() {
        buildWaveform();
        m_bpm = detectBpm();
    }));
}

void DeckPlayer::onDecoderError(QAudioDecoder::Error) {
    qWarning() << "[Deck" << m_deckIndex << "] Decoder error:" << m_decoder->errorString();
    setState(State::Empty);
    emit loadingError(m_decoder->errorString());
}

// ─── Waveform peak computation ────────────────────────────────────────────────
void DeckPlayer::buildWaveform() {
    const qint64 totalFrames = m_totalFrames.load(std::memory_order_acquire);
    if (m_pcm.empty() || totalFrames == 0) return;

    m_peaks.resize(kWaveformColumns, 0.0f);
    const qint64 framesPerCol = std::max(1LL, totalFrames / kWaveformColumns);

    for (int col = 0; col < kWaveformColumns; ++col) {
        qint64 start = col * framesPerCol;
        qint64 end   = std::min(start + framesPerCol, totalFrames);
        float peak = 0.0f;
        for (qint64 f = start; f < end; ++f) {
            int i = (int)(f * m_channels);
            float s = std::abs(m_pcm[i]);
            if (m_channels > 1) s = std::max(s, std::abs(m_pcm[i + 1]));
            peak = std::max(peak, s);
        }
        m_peaks[col] = peak;
    }
}

// ─── BPM via onset energy autocorrelation ────────────────────────────────────
float DeckPlayer::detectBpm() {
    const qint64 totalFrames = m_totalFrames.load(std::memory_order_acquire);
    if (m_pcm.empty() || m_sampleRate == 0 || totalFrames == 0) return 0.0f;

    // Compute RMS energy in 50ms windows (onset envelope)
    const int windowSize = m_sampleRate / 20; // 50ms
    const int hopSize    = windowSize / 2;
    std::vector<float> envelope;
    envelope.reserve(totalFrames / hopSize + 1);

    for (qint64 f = 0; f + windowSize <= totalFrames; f += hopSize) {
        float energy = 0.0f;
        for (int s = 0; s < windowSize; ++s) {
            float v = m_pcm[(f + s) * m_channels];
            energy += v * v;
        }
        envelope.push_back(std::sqrt(energy / windowSize));
    }

    if (envelope.size() < 64) return 0.0f;

    // Autocorrelation over BPM range 60-200
    const float sr_env = (float)m_sampleRate / hopSize; // envelope sample rate
    const int   minLag = (int)(sr_env * 60.0f / 200.0f); // 200 BPM
    const int   maxLag = (int)(sr_env * 60.0f / 60.0f);  // 60 BPM

    float bestCorr = -1.0f;
    int   bestLag  = minLag;
    const int N = std::min((int)envelope.size(), 2048);

    for (int lag = minLag; lag <= maxLag && lag < N; ++lag) {
        float corr = 0.0f;
        for (int i = 0; i < N - lag; ++i)
            corr += envelope[i] * envelope[i + lag];
        corr /= (N - lag);
        if (corr > bestCorr) {
            bestCorr = corr;
            bestLag = lag;
        }
    }

    float bpm = (sr_env * 60.0f) / bestLag;
    // Normalise to 60–200 range
    while (bpm < 60.0f)  bpm *= 2.0f;
    while (bpm > 200.0f) bpm /= 2.0f;

    return bpm;
}

// ─── Transport ────────────────────────────────────────────────────────────────
void DeckPlayer::play() {
    if (m_state != State::Ready && m_state != State::Paused) return;
    m_playing.store(true, std::memory_order_release);
    setState(State::Playing);
}

void DeckPlayer::pause() {
    if (m_state != State::Playing) return;
    m_playing.store(false, std::memory_order_release);
    setState(State::Paused);
}

void DeckPlayer::stop() {
    m_playing.store(false, std::memory_order_release);
    m_posFixed.store(0, std::memory_order_release);
    if (m_state == State::Playing || m_state == State::Paused)
        setState(State::Ready);
}

void DeckPlayer::togglePlayPause() {
    if (m_state == State::Playing) pause();
    else if (m_state == State::Ready || m_state == State::Paused) play();
}

// ─── Position ─────────────────────────────────────────────────────────────────
qint64 DeckPlayer::positionSamples() const {
    return m_posFixed.load(std::memory_order_relaxed) >> kSubFrameBits;
}

double DeckPlayer::positionSeconds() const {
    if (m_sampleRate == 0) return 0.0;
    return (double)positionSamples() / m_sampleRate;
}

double DeckPlayer::durationSeconds() const {
    if (m_sampleRate == 0) return 0.0;
    return (double)m_totalFrames.load(std::memory_order_relaxed) / m_sampleRate;
}

void DeckPlayer::seek(qint64 frame) {
    const qint64 total = m_totalFrames.load(std::memory_order_relaxed);
    frame = std::clamp(frame, 0LL, total > 0 ? total - 1 : 0LL);
    m_posFixed.store(frame << kSubFrameBits, std::memory_order_release);
    emit positionChanged(frame);
}

// ─── Cue / loop / hot cues ───────────────────────────────────────────────────
void DeckPlayer::setCuePoint() {
    m_cueFrame = positionSamples();
}

void DeckPlayer::jumpToCue() {
    seek(m_cueFrame);
}

void DeckPlayer::setLoop(bool enabled, qint64 inFrame, qint64 outFrame) {
    if (inFrame >= 0)  m_loopIn  = inFrame;
    if (outFrame >= 0) m_loopOut = outFrame;
    m_loopEnabled = enabled;
    // Publish to RT thread
    m_loopInAtomic.store(m_loopIn, std::memory_order_release);
    m_loopOutAtomic.store(m_loopOut, std::memory_order_release);
    m_loopEnabledAtomic.store(m_loopEnabled, std::memory_order_release);
}

void DeckPlayer::setHotCue(int index, qint64 frame) {
    if (index < 0 || index >= kHotCueCount) return;
    m_hotCues[index] = (frame < 0) ? positionSamples() : frame;
    emit hotCuesChanged();
}

void DeckPlayer::clearHotCue(int index) {
    if (index < 0 || index >= kHotCueCount) return;
    m_hotCues[index] = -1;
    emit hotCuesChanged();
}

void DeckPlayer::jumpToHotCue(int index) {
    if (index < 0 || index >= kHotCueCount) return;
    if (m_hotCues[index] >= 0) seek(m_hotCues[index]);
}

qint64 DeckPlayer::hotCue(int index) const {
    if (index < 0 || index >= kHotCueCount) return -1;
    return m_hotCues[index];
}

// ─── Speed / pitch ────────────────────────────────────────────────────────────
void DeckPlayer::setSpeed(float rate) {
    rate = std::clamp(rate, 0.1f, 3.0f);
    m_speed.store(rate, std::memory_order_release);
}

float DeckPlayer::speed() const {
    return m_speed.load(std::memory_order_relaxed);
}

void DeckPlayer::setGain(float g) {
    m_gain.store(std::clamp(g, 0.0f, 1.0f), std::memory_order_release);
}

void DeckPlayer::resetLevels() {
    m_levelL.store(0.0f, std::memory_order_relaxed);
    m_levelR.store(0.0f, std::memory_order_relaxed);
}

// ─── RT audio processing ──────────────────────────────────────────────────────
void DeckPlayer::processBlock(float* out, int frames, int outChannels, float gain) {
    if (!m_playing.load(std::memory_order_acquire)) return;

    // Streaming mode: read from ring buffer instead of PCM vector
    if (m_isStreaming) {
        processStreamBlock(out, frames, outChannels, gain);
        return;
    }

    const qint64 totalFrames = m_totalFrames.load(std::memory_order_acquire);
    if (totalFrames == 0 || m_pcm.empty())
        return;

    // Re-check m_playing after acquiring totalFrames — unload() may have
    // set m_playing=false + m_totalFrames=0 between our first check and here.
    if (!m_playing.load(std::memory_order_acquire)) return;

    // Combine crossfader gain with per-deck volume fader
    const float finalGain = gain * m_gain.load(std::memory_order_relaxed);

    const float speed    = m_speed.load(std::memory_order_relaxed);
    const qint64 stepFP  = (qint64)(speed * kSubFrameUnit + 0.5f);
    const bool loopOn    = m_loopEnabledAtomic.load(std::memory_order_acquire);
    const qint64 loopIn  = m_loopInAtomic.load(std::memory_order_relaxed);
    const qint64 loopOut = m_loopOutAtomic.load(std::memory_order_relaxed);

    qint64 posFP = m_posFixed.load(std::memory_order_relaxed);
    float  pkL   = 0.0f, pkR = 0.0f;

    for (int f = 0; f < frames; ++f) {
        qint64 iFrame = posFP >> kSubFrameBits;

        // Handle loop wrap-around
        if (loopOn && loopOut > 0 && iFrame >= loopOut) {
            posFP = loopIn << kSubFrameBits;
            iFrame = loopIn;
        }

        // End of file
        if (iFrame >= totalFrames - 1) {
            m_playing.store(false, std::memory_order_relaxed);
            break;
        }

        // Linear interpolation (sub-frame)
        const float frac = (float)(posFP & (kSubFrameUnit - 1)) / kSubFrameUnit;
        const qint64 i0 = iFrame * m_channels;
        const qint64 i1 = i0 + m_channels;

        // Guard: bail if pcm was cleared by unload() on another thread
        if (i1 + m_channels > (qint64)m_pcm.size()) {
            m_playing.store(false, std::memory_order_relaxed);
            break;
        }

        const float sL0 = m_pcm[i0];
        const float sL1 = m_pcm[i1];
        float sL  = sL0 + frac * (sL1 - sL0);

        const float sR0 = (m_channels > 1) ? m_pcm[i0 + 1] : sL0;
        const float sR1 = (m_channels > 1) ? m_pcm[i1 + 1] : sL1;
        float sR  = sR0 + frac * (sR1 - sR0);

        // ── 3-band EQ (lazy coef recompute on dirty flag) ─────────────────────
        for (int b = 0; b < 3; ++b) {
            if (m_eqDirty[b].exchange(false, std::memory_order_acq_rel)) {
                const double dB = (double)m_eqGainDb[b].load(std::memory_order_relaxed);
                const double sr = (double)m_sampleRate;
                if      (b == 0) m_eqCoef[b] = makeLowShelf (  100.0,      sr, dB);
                else if (b == 1) m_eqCoef[b] = makePeakingEQ( 1000.0, 1.5, sr, dB);
                else             m_eqCoef[b] = makeHighShelf (10000.0,      sr, dB);
            }
            sL = (float)processBq(m_eqCoef[b], m_eqState[b][0], (double)sL);
            sR = (float)processBq(m_eqCoef[b], m_eqState[b][1], (double)sR);
        }

        // Write to output (additive mix)
        if (outChannels >= 2) {
            out[f * outChannels + 0] += sL * finalGain;
            out[f * outChannels + 1] += sR * finalGain;
        } else {
            out[f] += (sL + sR) * 0.5f * finalGain;
        }

        // Measure post-gain levels so VU meters respond to volume fader
        pkL = std::max(pkL, std::abs(sL * finalGain));
        pkR = std::max(pkR, std::abs(sR * finalGain));
        posFP += stepFP;
    }

    // Update peak levels (relaxed — UI reads them at 50ms intervals)
    m_levelL.store(pkL, std::memory_order_relaxed);
    m_levelR.store(pkR, std::memory_order_relaxed);
    m_posFixed.store(posFP, std::memory_order_relaxed);
}

// ─── Streaming processBlock (reads from ring buffer) ──────────────────────────
void DeckPlayer::processStreamBlock(float* out, int frames, int outChannels, float gain) {
    if (!m_streamReader) return;

    const float finalGain = gain * m_gain.load(std::memory_order_relaxed);

    // Read decoded PCM from stream ring buffer into local temp buffer
    // Max 2048 stereo frames per call to avoid stack overflow
    float temp[4096];
    const int readFrames = std::min(frames, 2048);
    int got = m_streamReader->readPcm(temp, readFrames);
    if (got <= 0) return;

    float pkL = 0.0f, pkR = 0.0f;

    for (int f = 0; f < got; ++f) {
        float sL = temp[f * 2];
        float sR = temp[f * 2 + 1];

        // Apply 3-band EQ (same coefficients as file mode)
        for (int b = 0; b < 3; ++b) {
            if (m_eqDirty[b].exchange(false, std::memory_order_acq_rel)) {
                const double dB = (double)m_eqGainDb[b].load(std::memory_order_relaxed);
                const double sr = 48000.0;  // stream always resampled to 48 kHz
                if      (b == 0) m_eqCoef[b] = makeLowShelf (  100.0,      sr, dB);
                else if (b == 1) m_eqCoef[b] = makePeakingEQ( 1000.0, 1.5, sr, dB);
                else             m_eqCoef[b] = makeHighShelf (10000.0,      sr, dB);
            }
            sL = (float)processBq(m_eqCoef[b], m_eqState[b][0], (double)sL);
            sR = (float)processBq(m_eqCoef[b], m_eqState[b][1], (double)sR);
        }

        if (outChannels >= 2) {
            out[f * outChannels + 0] += sL * finalGain;
            out[f * outChannels + 1] += sR * finalGain;
        } else {
            out[f] += (sL + sR) * 0.5f * finalGain;
        }

        // Measure post-gain levels so VU meters respond to volume fader
        pkL = std::max(pkL, std::abs(sL * finalGain));
        pkR = std::max(pkR, std::abs(sR * finalGain));
    }

    m_levelL.store(pkL, std::memory_order_relaxed);
    m_levelR.store(pkR, std::memory_order_relaxed);
}

// ─── State helper ─────────────────────────────────────────────────────────────
void DeckPlayer::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

// ─── EOF detection (Qt thread, 50ms poll) ─────────────────────────────────────
void DeckPlayer::onEofPoll() {
    // If state is Playing but RT thread has stopped (m_playing == false) → EOF reached
    if (m_state == State::Playing && !m_playing.load(std::memory_order_acquire)) {
        setState(State::Ready);
        emit finished();
    }
}

} // namespace M1
