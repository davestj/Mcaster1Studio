#include "CartPlayer.h"
#include <QAudioFormat>
#include <QUrl>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace M1 {

CartPlayer::CartPlayer(QObject* parent)
    : QObject(parent)
{
    m_eofTimer = new QTimer(this);
    m_eofTimer->setInterval(50);
    connect(m_eofTimer, &QTimer::timeout, this, &CartPlayer::onEofPoll);
}

CartPlayer::~CartPlayer() {
    unload();
}

// ── File loading ─────────────────────────────────────────────────────────────

void CartPlayer::loadFile(const QString& path) {
    unload();

    m_path = path;
    setState(State::Loading);

    // Set up decoder for float32 stereo @ 48 kHz
    m_decoder = new QAudioDecoder(this);
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Float);
    m_decoder->setAudioFormat(fmt);

    connect(m_decoder, &QAudioDecoder::bufferReady,
            this, &CartPlayer::onDecoderBufferReady);
    connect(m_decoder, &QAudioDecoder::finished,
            this, &CartPlayer::onDecoderFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
            this, &CartPlayer::onDecoderError);

    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
}

void CartPlayer::unload() {
    m_playing.store(false, std::memory_order_relaxed);
    m_eofTimer->stop();

    if (m_decoder) {
        m_decoder->stop();
        m_decoder->deleteLater();
        m_decoder = nullptr;
    }

    m_pcm.clear();
    m_totalFrames = 0;
    m_pos.store(0, std::memory_order_relaxed);
    m_peak.store(0.0f, std::memory_order_relaxed);
    m_path.clear();
    setState(State::Empty);
}

// ── Transport ────────────────────────────────────────────────────────────────

void CartPlayer::play() {
    if (m_state != State::Ready && m_state != State::Playing) return;
    m_pos.store(0, std::memory_order_release);  // restart from beginning
    m_playing.store(true, std::memory_order_release);
    setState(State::Playing);
    m_eofTimer->start();
}

void CartPlayer::stop() {
    m_playing.store(false, std::memory_order_relaxed);
    m_pos.store(0, std::memory_order_relaxed);
    m_eofTimer->stop();
    if (m_state == State::Playing)
        setState(State::Ready);
}

double CartPlayer::durationSeconds() const {
    if (m_sampleRate <= 0 || m_totalFrames <= 0) return 0.0;
    return static_cast<double>(m_totalFrames) / m_sampleRate;
}

// ── RT audio processing ──────────────────────────────────────────────────────

void CartPlayer::processBlock(float* out, int frames, int outChannels) noexcept {
    if (!m_playing.load(std::memory_order_acquire)) return;
    if (m_pcm.empty() || m_totalFrames == 0) return;

    const float g = m_gain.load(std::memory_order_relaxed);
    int pos = m_pos.load(std::memory_order_relaxed);
    float pk = 0.0f;

    for (int f = 0; f < frames; ++f) {
        if (pos >= m_totalFrames) {
            m_playing.store(false, std::memory_order_relaxed);
            break;
        }

        const int srcOff = pos * m_channels;
        const float sL = m_pcm[srcOff] * g;
        const float sR = (m_channels > 1) ? m_pcm[srcOff + 1] * g : sL;

        out[f * outChannels]     += sL;
        if (outChannels > 1)
            out[f * outChannels + 1] += sR;

        const float absPk = std::max(std::fabs(sL), std::fabs(sR));
        if (absPk > pk) pk = absPk;

        ++pos;
    }

    m_pos.store(pos, std::memory_order_relaxed);
    m_peak.store(pk, std::memory_order_relaxed);
}

// ── Decoder slots ────────────────────────────────────────────────────────────

void CartPlayer::onDecoderBufferReady() {
    if (!m_decoder) return;
    const QAudioBuffer buf = m_decoder->read();
    if (!buf.isValid()) return;

    const float* src = buf.constData<float>();
    const int    n   = buf.frameCount() * buf.format().channelCount();
    m_pcm.insert(m_pcm.end(), src, src + n);
    m_channels   = buf.format().channelCount();
    m_sampleRate = buf.format().sampleRate();
}

void CartPlayer::onDecoderFinished() {
    m_totalFrames = m_channels > 0
        ? static_cast<int>(m_pcm.size()) / m_channels
        : 0;

    if (m_decoder) {
        m_decoder->deleteLater();
        m_decoder = nullptr;
    }

    setState(State::Ready);
    emit loadingFinished();
    qInfo() << "[CartPlayer] Loaded:" << m_path
            << m_totalFrames << "frames @" << m_sampleRate << "Hz";
}

void CartPlayer::onDecoderError(QAudioDecoder::Error error) {
    Q_UNUSED(error)
    const QString msg = m_decoder ? m_decoder->errorString() : "Unknown error";
    qWarning() << "[CartPlayer] Decode error:" << msg;
    emit loadingError(msg);
    setState(State::Empty);
}

void CartPlayer::onEofPoll() {
    if (!m_playing.load(std::memory_order_relaxed) && m_state == State::Playing) {
        m_eofTimer->stop();
        setState(State::Ready);
        emit playbackFinished();
    }
}

void CartPlayer::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

} // namespace M1
