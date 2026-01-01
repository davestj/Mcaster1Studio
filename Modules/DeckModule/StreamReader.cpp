#include "StreamReader.h"
#include <QSslSocket>
#include <QTcpSocket>
#include <QDebug>
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

namespace M1 {

StreamReader::StreamReader(QObject* parent)
    : QThread(parent)
    , m_ring(static_cast<size_t>(kRingFrames) * 2, 0.0f)
{}

StreamReader::~StreamReader() {
    close();
    wait(5000);
}

void StreamReader::open(const QUrl& url) {
    if (m_running.load()) close();
    m_url = url;
    m_ringW.store(0, std::memory_order_relaxed);
    m_ringR.store(0, std::memory_order_relaxed);
    m_running.store(true, std::memory_order_release);
    start();
}

void StreamReader::close() {
    m_running.store(false, std::memory_order_release);
}

// ─── RT-safe ring buffer read ────────────────────────────────────────────────
int StreamReader::readPcm(float* out, int maxFrames) noexcept {
    const int w = m_ringW.load(std::memory_order_acquire);
    const int r = m_ringR.load(std::memory_order_relaxed);
    const int avail = w - r;
    const int frames = std::min(avail, maxFrames);
    if (frames <= 0) return 0;

    for (int i = 0; i < frames; ++i) {
        const int off = ((r + i) & kRingMask) * 2;
        out[i * 2]     = m_ring[off];
        out[i * 2 + 1] = m_ring[off + 1];
    }
    m_ringR.store(r + frames, std::memory_order_release);
    return frames;
}

int StreamReader::bufferedFrames() const noexcept {
    return m_ringW.load(std::memory_order_acquire)
         - m_ringR.load(std::memory_order_acquire);
}

void StreamReader::ringWrite(const float* stereoData, int frames) {
    const int w = m_ringW.load(std::memory_order_relaxed);
    const int r = m_ringR.load(std::memory_order_acquire);
    const int used = w - r;
    if (used + frames > kRingFrames) return; // drop if full

    for (int i = 0; i < frames; ++i) {
        const int off = ((w + i) & kRingMask) * 2;
        m_ring[off]     = stereoData[i * 2];
        m_ring[off + 1] = stereoData[i * 2 + 1];
    }
    m_ringW.store(w + frames, std::memory_order_release);
}

// ─── M3U / PLS playlist resolver ─────────────────────────────────────────────
QUrl StreamReader::resolvePlaylist(const QUrl& url) {
    const QString path = url.path().toLower();
    if (!path.endsWith(".m3u") && !path.endsWith(".m3u8") && !path.endsWith(".pls"))
        return url;

    const bool ssl = (url.scheme() == "https");
    const int port = url.port(ssl ? 443 : 80);

    // Fetch the playlist over HTTP(S)
    std::unique_ptr<QAbstractSocket> sock;
    if (ssl) {
        auto* s = new QSslSocket;
        s->connectToHostEncrypted(url.host(), static_cast<quint16>(port));
        if (!s->waitForEncrypted(10000)) { delete s; return url; }
        sock.reset(s);
    } else {
        auto* s = new QTcpSocket;
        s->connectToHost(url.host(), static_cast<quint16>(port));
        if (!s->waitForConnected(10000)) { delete s; return url; }
        sock.reset(s);
    }

    QByteArray req = "GET " + url.path().toUtf8();
    if (url.hasQuery()) req += "?" + url.query().toUtf8();
    req += " HTTP/1.0\r\n"
           "Host: " + url.host().toUtf8() + ":" + QByteArray::number(port) + "\r\n"
           "User-Agent: Mcaster1Studio/0.1.0\r\n"
           "Connection: close\r\n\r\n";
    sock->write(req);
    sock->waitForBytesWritten(5000);

    QByteArray response;
    while (sock->waitForReadyRead(5000))
        response.append(sock->readAll());
    sock->disconnectFromHost();

    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart < 0) return url;
    QByteArray body = response.mid(bodyStart + 4);

    if (path.endsWith(".pls")) {
        for (const QByteArray& line : body.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith("File1="))
                return QUrl(QString::fromUtf8(trimmed.mid(6)));
        }
    } else {
        for (const QByteArray& line : body.split('\n')) {
            QByteArray trimmed = line.trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith('#'))
                return QUrl(QString::fromUtf8(trimmed));
        }
    }

    return url;
}

// ─── Main decode thread ──────────────────────────────────────────────────────
void StreamReader::run() {
    // Resolve M3U/PLS playlists to the actual stream URL
    QUrl resolved = resolvePlaylist(m_url);
    qInfo() << "[StreamReader] Opening:" << resolved.toString();

    // ── FFmpeg: open HTTP stream ─────────────────────────────────
    AVFormatContext* fmtCtx = nullptr;
    AVDictionary* opts = nullptr;

    // Enable ICY metadata + ICY 2.2 + reconnect
    av_dict_set(&opts, "icy",                  "1", 0);
    av_dict_set(&opts, "user_agent",           "Mcaster1Studio/0.1.0", 0);
    av_dict_set(&opts, "headers",              "Icy-Version: 2.2\r\n", 0);
    av_dict_set(&opts, "reconnect",            "1", 0);
    av_dict_set(&opts, "reconnect_streamed",   "1", 0);
    av_dict_set(&opts, "reconnect_delay_max",  "10", 0);
    av_dict_set(&opts, "timeout",              "15000000", 0);  // 15s in µs

    QByteArray urlStr = resolved.toString().toUtf8();
    int ret = avformat_open_input(&fmtCtx, urlStr.constData(), nullptr, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, err, sizeof(err));
        emit streamError(QString("Failed to open stream: %1").arg(err));
        m_running.store(false);
        return;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        emit streamError("Failed to find stream info");
        avformat_close_input(&fmtCtx);
        m_running.store(false);
        return;
    }

    int audioIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioIdx < 0) {
        emit streamError("No audio stream found");
        avformat_close_input(&fmtCtx);
        m_running.store(false);
        return;
    }

    // ── Emit initial metadata (ICY headers + ICY 2.2 headers) ────
    {
        QMap<QString, QString> meta;
        const AVDictionaryEntry* e = nullptr;
        while ((e = av_dict_iterate(fmtCtx->metadata, e)))
            meta.insert(QString::fromUtf8(e->key), QString::fromUtf8(e->value));

        QString name = meta.value("icy-name", meta.value("title", resolved.host()));
        QString genre = meta.value("icy-genre", "");
        QString ct = "";
        if (fmtCtx->iformat)
            ct = QString::fromUtf8(fmtCtx->iformat->long_name);

        emit connected(name, genre, ct);
        if (!meta.isEmpty())
            emit metadataChanged(meta);
    }

    // ── Set up codec decoder ─────────────────────────────────────
    AVCodecParameters* par = fmtCtx->streams[audioIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        emit streamError("Unsupported audio codec");
        avformat_close_input(&fmtCtx);
        m_running.store(false);
        return;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, par);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        emit streamError("Failed to open audio decoder");
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        m_running.store(false);
        return;
    }

    // ── Set up resampler → float32 stereo @ 48 kHz ──────────────
    SwrContext* swrCtx = nullptr;
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    ret = swr_alloc_set_opts2(&swrCtx,
        &outLayout, AV_SAMPLE_FMT_FLT, 48000,
        &codecCtx->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
        0, nullptr);
    if (ret < 0 || swr_init(swrCtx) < 0) {
        emit streamError("Failed to init resampler");
        swr_free(&swrCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        m_running.store(false);
        return;
    }

    qInfo() << "[StreamReader] Decoding:" << codec->long_name
            << par->sample_rate << "Hz" << par->ch_layout.nb_channels << "ch";

    // ── Decode loop ──────────────────────────────────────────────
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    QString lastTitle;

    while (m_running.load(std::memory_order_relaxed)) {
        ret = av_read_frame(fmtCtx, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF || ret == AVERROR_EXIT)
                break;
            // Transient error — try to continue
            av_packet_unref(pkt);
            continue;
        }

        if (pkt->stream_index != audioIdx) {
            av_packet_unref(pkt);
            continue;
        }

        // Check for metadata updates (ICY StreamTitle, ICY 2.2 fields)
        if (fmtCtx->event_flags & AVFMT_EVENT_FLAG_METADATA_UPDATED) {
            fmtCtx->event_flags &= ~AVFMT_EVENT_FLAG_METADATA_UPDATED;
            QMap<QString, QString> meta;
            const AVDictionaryEntry* e = nullptr;
            while ((e = av_dict_iterate(fmtCtx->metadata, e)))
                meta.insert(QString::fromUtf8(e->key), QString::fromUtf8(e->value));

            QString title = meta.value("StreamTitle", meta.value("title", ""));
            if (!title.isEmpty() && title != lastTitle) {
                lastTitle = title;
                emit metadataChanged(meta);
            }
        }

        // Decode
        avcodec_send_packet(codecCtx, pkt);
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            // Resample to float32 stereo @ 48 kHz
            int outSamples = swr_get_out_samples(swrCtx, frame->nb_samples);
            if (outSamples <= 0) continue;

            std::vector<float> outBuf(static_cast<size_t>(outSamples) * 2);
            uint8_t* outPtr = reinterpret_cast<uint8_t*>(outBuf.data());
            int converted = swr_convert(swrCtx,
                &outPtr, outSamples,
                const_cast<const uint8_t**>(frame->extended_data),
                frame->nb_samples);

            if (converted > 0)
                ringWrite(outBuf.data(), converted);
        }

        av_packet_unref(pkt);
    }

    // ── Cleanup ──────────────────────────────────────────────────
    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swrCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    m_running.store(false, std::memory_order_release);
    emit disconnected();
    qInfo() << "[StreamReader] Disconnected.";
}

} // namespace M1
