#include "PodcastModule.h"
#include "PodcastWidget.h"
#include "AudioBuffer.h"
#include "IPlugin.h"
#include <QSettings>
#include <QTemporaryFile>
#include <QFile>
#include <QDataStream>
#include <QDir>
#include <QTimer>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace M1 {

// ─── RingBuffer ───────────────────────────────────────────────────────────────

int PodcastModule::RingBuffer::write(const float* src, int n) noexcept {
    const int wi   = writeIdx.load(std::memory_order_relaxed);
    const int ri   = readIdx.load(std::memory_order_acquire);
    const int free = kRingSize - ((wi - ri + kRingSize) % kRingSize) - 1;
    const int toW  = std::min(n, free);
    if (toW <= 0) return 0;

    const int end = (wi + toW) % kRingSize;
    if (end > wi) {
        std::memcpy(data.data() + wi, src, static_cast<size_t>(toW) * sizeof(float));
    } else {
        // Wrap
        const int first = kRingSize - wi;
        std::memcpy(data.data() + wi, src,          static_cast<size_t>(first)       * sizeof(float));
        std::memcpy(data.data(),      src + first,  static_cast<size_t>(toW - first) * sizeof(float));
    }
    writeIdx.store((wi + toW) % kRingSize, std::memory_order_release);
    return toW;
}

int PodcastModule::RingBuffer::read(float* dst, int n) noexcept {
    const int ri  = readIdx.load(std::memory_order_relaxed);
    const int wi  = writeIdx.load(std::memory_order_acquire);
    const int avail = (wi - ri + kRingSize) % kRingSize;
    const int toR   = std::min(n, avail);
    if (toR <= 0) return 0;

    const int end = (ri + toR) % kRingSize;
    if (end > ri) {
        std::memcpy(dst, data.data() + ri, static_cast<size_t>(toR) * sizeof(float));
    } else {
        const int first = kRingSize - ri;
        std::memcpy(dst,         data.data() + ri, static_cast<size_t>(first)       * sizeof(float));
        std::memcpy(dst + first, data.data(),      static_cast<size_t>(toR - first) * sizeof(float));
    }
    readIdx.store((ri + toR) % kRingSize, std::memory_order_release);
    return toR;
}

int PodcastModule::RingBuffer::available() const noexcept {
    const int wi = writeIdx.load(std::memory_order_acquire);
    const int ri = readIdx.load(std::memory_order_relaxed);
    return (wi - ri + kRingSize) % kRingSize;
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PodcastModule::PodcastModule(QObject* parent)
    : IModule(parent)
{
    qRegisterMetaType<M1::PodcastModule::State>("M1::PodcastModule::State");

    m_trackNames.reserve(kMaxTracks);
    for (int i = 0; i < kMaxTracks; ++i) {
        m_trackNames.append(QString("Track %1").arg(i + 1));
        m_trackLevel[i].store(0.0f, std::memory_order_relaxed);
        m_tempFiles[i]  = nullptr;
        m_sampleCount[i] = 0;
    }
}

PodcastModule::~PodcastModule() {
    shutdown();
}

// ─── IModule lifecycle ────────────────────────────────────────────────────────

void PodcastModule::initialize() {
    m_drainTimer = new QTimer(this);
    m_drainTimer->setInterval(20); // drain every 20ms
    connect(m_drainTimer, &QTimer::timeout, this, &PodcastModule::drainRingBuffers);
    qInfo() << "[PodcastModule] Initialized with" << kMaxTracks << "tracks.";
}

void PodcastModule::shutdown() {
    if (m_drainTimer) {
        m_drainTimer->stop();
    }
    stop();
    qInfo() << "[PodcastModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────

QWidget* PodcastModule::createWidget(QWidget* parent) {
    m_widget = new PodcastWidget(this, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    return m_widget;
}

// ─── RT audio thread ──────────────────────────────────────────────────────────
//
// Capture strategy: each AudioBuffer channel is mapped to one podcast track.
// Track 0 = channel 0, Track 1 = channel 1, Track 2 = channel 2, Track 3 = channel 3.
// If the input has fewer channels than tracks, extra tracks receive silence (nothing).
//
// Level meter: computed from peak absolute value per channel, stored atomically.
// Pass-through: `in` is copied to `out` (monitoring pass-through).
//
// NO Qt API calls, NO allocation, NO mutex waits.

void PodcastModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    // Always pass audio through for monitoring
    if (in.isValid && out.isValid) {
        const int total = std::min(in.totalSamples(), out.totalSamples());
        for (int i = 0; i < total; ++i)
            out.data[i] += in.data[i];
    }

    if (!in.isValid || in.frames <= 0) return;

    // Cache audio format for WAV export header
    m_channels.store(in.channels, std::memory_order_relaxed);
    // Use atomic<double> store via bit-cast trick — store as int64 bit pattern
    // is not portable so we store as relaxed int. For sampleRate we use a
    // separate simple cast: just store as double (no strict requirement for
    // atomicity — read in Qt thread after recording stops).
    // NOTE: std::atomic<double> is used for the sample rate member.
    m_sampleRate.store(in.sampleRate, std::memory_order_relaxed);

    // Level meters — always update
    for (int track = 0; track < kMaxTracks; ++track) {
        const int ch = track % in.channels;
        float peak = 0.0f;
        for (int f = 0; f < in.frames; ++f) {
            const float s = std::fabs(in.data[f * in.channels + ch]);
            if (s > peak) peak = s;
        }
        // Simple peak hold with 200ms release
        float cur = m_trackLevel[track].load(std::memory_order_relaxed);
        const float release = 0.999f;
        cur = (peak > cur) ? peak : release * cur;
        m_trackLevel[track].store(cur, std::memory_order_relaxed);
    }

    // Only capture when recording
    if (!m_recording.load(std::memory_order_acquire)) return;

    // Write per-track mono channel data into ring buffers.
    // We de-interleave one channel per track.
    for (int track = 0; track < kMaxTracks; ++track) {
        const int ch = track % in.channels;
        // De-interleave into a stack-local buffer (no allocation needed for
        // moderate block sizes; 4096 frames * 4 bytes = 16KB per track, fine
        // for stack at typical 128–1024 frame block sizes).
        // For safety, cap to 4096 frames per block (audio engines rarely exceed this).
        const int frames = std::min(in.frames, 4096);

        // Stack allocation — acceptable for RT thread at reasonable block sizes.
        // This is a deliberate design choice: no heap, no mutex, just stack.
        float scratch[4096];
        for (int f = 0; f < frames; ++f)
            scratch[f] = in.data[f * in.channels + ch];

        m_rings[track].write(scratch, frames);
    }
}

// ─── Ring buffer drain (Qt timer thread) ─────────────────────────────────────

void PodcastModule::drainRingBuffers() {
    if (m_state.load(std::memory_order_relaxed) != static_cast<int>(State::Recording))
        return;

    for (int track = 0; track < kMaxTracks; ++track) {
        if (!m_tempFiles[track] || !m_tempFiles[track]->isOpen()) continue;

        int avail = m_rings[track].available();
        while (avail > 0) {
            const int toRead = std::min(avail, kDrainBuf);
            const int read   = m_rings[track].read(m_drainScratch, toRead);
            if (read <= 0) break;

            // Write raw float32 PCM to temp file
            qint64 written = m_tempFiles[track]->write(
                reinterpret_cast<const char*>(m_drainScratch),
                static_cast<qint64>(read) * sizeof(float)
            );
            if (written < 0) {
                qWarning() << "[PodcastModule] Write error on track" << track
                           << m_tempFiles[track]->errorString();
                break;
            }
            m_sampleCount[track] += read;
            avail = m_rings[track].available();
        }
    }
}

// ─── Session control (Qt thread) ─────────────────────────────────────────────

void PodcastModule::arm() {
    if (m_state.load() != static_cast<int>(State::Idle)) return;
    m_state.store(static_cast<int>(State::Armed), std::memory_order_relaxed);
    emit stateChanged(State::Armed);
}

void PodcastModule::record() {
    const int s = m_state.load();
    if (s != static_cast<int>(State::Armed) && s != static_cast<int>(State::Paused)) return;

    if (s == static_cast<int>(State::Armed)) {
        // Fresh start — open temp files and reset
        openTempFiles();
        m_elapsed.restart();
        m_pausedMs = 0;
        m_chapters.clear();
    } else {
        // Resume from pause — restart elapsed without clearing chapters
        m_elapsed.restart();
    }

    m_recording.store(true, std::memory_order_release);
    m_state.store(static_cast<int>(State::Recording), std::memory_order_relaxed);

    if (m_drainTimer) m_drainTimer->start();

    emit stateChanged(State::Recording);
}

void PodcastModule::pause() {
    if (m_state.load() != static_cast<int>(State::Recording)) return;

    m_recording.store(false, std::memory_order_release);
    m_pausedMs += m_elapsed.elapsed();

    if (m_drainTimer) m_drainTimer->stop();

    // Drain any remaining buffered samples
    drainRingBuffers();

    m_state.store(static_cast<int>(State::Paused), std::memory_order_relaxed);
    emit stateChanged(State::Paused);
}

void PodcastModule::stop() {
    const int s = m_state.load();
    if (s == static_cast<int>(State::Idle)) return;

    m_recording.store(false, std::memory_order_release);

    if (m_drainTimer) m_drainTimer->stop();

    // Final drain
    if (s == static_cast<int>(State::Recording) || s == static_cast<int>(State::Paused)) {
        drainRingBuffers();
    }

    closeTempFiles();

    m_pausedMs = 0;
    m_state.store(static_cast<int>(State::Idle), std::memory_order_relaxed);
    emit stateChanged(State::Idle);
}

PodcastModule::State PodcastModule::sessionState() const {
    return static_cast<State>(m_state.load(std::memory_order_relaxed));
}

qint64 PodcastModule::elapsedMs() const {
    const int s = m_state.load(std::memory_order_relaxed);
    if (s == static_cast<int>(State::Recording)) {
        return m_pausedMs + m_elapsed.elapsed();
    }
    return m_pausedMs;
}

// ─── Chapter management ───────────────────────────────────────────────────────

void PodcastModule::addChapter(const QString& title) {
    PodcastChapter ch;
    ch.positionMs = elapsedMs();
    ch.title      = title.isEmpty() ? QString("Chapter %1").arg(m_chapters.size() + 1) : title;
    m_chapters.append(ch);
}

void PodcastModule::removeChapter(int index) {
    if (index >= 0 && index < m_chapters.size())
        m_chapters.removeAt(index);
}

// ─── Track names ──────────────────────────────────────────────────────────────

QString PodcastModule::trackName(int track) const {
    if (track >= 0 && track < m_trackNames.size())
        return m_trackNames[track];
    return QString();
}

void PodcastModule::setTrackName(int track, const QString& name) {
    if (track >= 0 && track < m_trackNames.size())
        m_trackNames[track] = name;
}

// ─── Temp file management ─────────────────────────────────────────────────────

void PodcastModule::openTempFiles() {
    closeTempFiles();
    for (int i = 0; i < kMaxTracks; ++i) {
        auto* f = new QTemporaryFile(this);
        f->setFileTemplate(QString("%1/mcaster1_podcast_track%2_XXXXXX.pcm")
                           .arg(QDir::tempPath()).arg(i));
        if (!f->open()) {
            qWarning() << "[PodcastModule] Failed to open temp file for track" << i
                       << f->errorString();
            delete f;
            m_tempFiles[i] = nullptr;
        } else {
            m_tempFiles[i]  = f;
            m_sampleCount[i] = 0;
        }
    }
}

void PodcastModule::closeTempFiles() {
    for (int i = 0; i < kMaxTracks; ++i) {
        if (m_tempFiles[i]) {
            m_tempFiles[i]->close();
            // Do NOT delete — QTemporaryFile owned by `this` (QObject parent)
            // and will be auto-deleted. We set to nullptr for re-use detection.
            // Actually, since we set parent=this above, it lives as long as module.
            // Re-use pattern: delete old and create new each session.
            delete m_tempFiles[i];
            m_tempFiles[i] = nullptr;
        }
        m_sampleCount[i] = 0;
    }
}

// ─── Export ───────────────────────────────────────────────────────────────────

bool PodcastModule::writePcmHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount) {
    // Standard WAV header for 32-bit float PCM
    const quint32 dataSize    = static_cast<quint32>(sampleCount * sizeof(float));
    const quint32 fmtChunkSz  = 18;    // PCM = 16, but float PCM needs 18 (extra 2 bytes)
    const quint32 riffSz      = 4 + (8 + fmtChunkSz) + (8 + dataSize);
    const quint16 numChan     = static_cast<quint16>(channels);
    const quint32 sRate       = static_cast<quint32>(sampleRate);
    const quint16 bitsPerSamp = 32;
    const quint32 byteRate    = sRate * numChan * (bitsPerSamp / 8);
    const quint16 blockAlign  = static_cast<quint16>(numChan * (bitsPerSamp / 8));
    const quint16 audioFmt    = 3;    // IEEE float

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    // RIFF chunk
    file.write("RIFF", 4);
    ds << riffSz;
    file.write("WAVE", 4);

    // fmt  chunk
    file.write("fmt ", 4);
    ds << fmtChunkSz;
    ds << audioFmt;
    ds << numChan;
    ds << sRate;
    ds << byteRate;
    ds << blockAlign;
    ds << bitsPerSamp;
    ds << quint16(0);   // cbSize for non-PCM

    // data chunk
    file.write("data", 4);
    ds << dataSize;

    return file.error() == QFile::NoError;
}

bool PodcastModule::exportWav(const QString& filePath, QString& errorOut) {
    // Export first active track (track 0) as the primary mix.
    // A full implementation would mix all tracks; here we export track 0.
    // TODO: implement multi-track mixdown.

    if (!m_tempFiles[0] || m_sampleCount[0] == 0) {
        errorOut = "No recorded audio to export.";
        return false;
    }

    const int    channels   = m_channels.load(std::memory_order_relaxed);
    const double sampleRate = m_sampleRate.load(std::memory_order_relaxed);

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorOut = QString("Cannot open output file: %1").arg(outFile.errorString());
        return false;
    }

    // Write WAV header
    if (!writePcmHeader(outFile, channels, sampleRate, m_sampleCount[0])) {
        errorOut = "Failed to write WAV header.";
        return false;
    }

    // Re-read temp file and append PCM data
    if (!m_tempFiles[0]->seek(0)) {
        errorOut = "Failed to seek temp file.";
        return false;
    }

    static constexpr int kCopyBuf = 65536;
    QByteArray buf(kCopyBuf, 0);
    qint64 remaining = m_sampleCount[0] * static_cast<qint64>(sizeof(float));
    while (remaining > 0) {
        const qint64 toRead = std::min(remaining, static_cast<qint64>(kCopyBuf));
        const qint64 nRead  = m_tempFiles[0]->read(buf.data(), toRead);
        if (nRead <= 0) break;
        const qint64 nWrit  = outFile.write(buf.data(), nRead);
        if (nWrit != nRead) {
            errorOut = "Write error during export.";
            return false;
        }
        remaining -= nRead;
    }

    outFile.close();
    emit exportFinished(true, filePath);
    return true;
}

void PodcastModule::exportMp3(const QString& filePath) {
    qWarning() << "[PodcastModule] MP3 export not yet implemented. Path:" << filePath;
    // TODO: Integrate LAME encoder (external/lame/) for MP3 export.
    emit exportFinished(false, filePath);
}

void PodcastModule::exportOpus(const QString& filePath) {
    qWarning() << "[PodcastModule] Opus export not yet implemented. Path:" << filePath;
    // TODO: Integrate libopusenc via vcpkg for Opus export.
    emit exportFinished(false, filePath);
}

void PodcastModule::exportFlac(const QString& filePath) {
    qWarning() << "[PodcastModule] FLAC export not yet implemented. Path:" << filePath;
    // TODO: Integrate libFLAC via vcpkg for FLAC export.
    emit exportFinished(false, filePath);
}

// ─── State persistence ────────────────────────────────────────────────────────

void PodcastModule::saveState(QSettings& s) {
    s.beginGroup("PodcastModule");
    s.beginWriteArray("trackNames", kMaxTracks);
    for (int i = 0; i < kMaxTracks; ++i) {
        s.setArrayIndex(i);
        s.setValue("name", m_trackNames.value(i, QString("Track %1").arg(i + 1)));
    }
    s.endArray();
    s.endGroup();
}

void PodcastModule::loadState(QSettings& s) {
    s.beginGroup("PodcastModule");
    const int n = s.beginReadArray("trackNames");
    for (int i = 0; i < std::min(n, kMaxTracks); ++i) {
        s.setArrayIndex(i);
        m_trackNames[i] = s.value("name", QString("Track %1").arg(i + 1)).toString();
    }
    s.endArray();
    s.endGroup();
}

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────

static Mcaster1PluginInfo s_podcastInfo {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.podcast",
    "Podcast",
    "1.0.0",
    "podcast,church",
    "module",
    "Mcaster1",
    "Multi-track podcast session recorder with chapter markers and WAV export"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_podcast_plugin_info() {
    return &s_podcastInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_podcast_create(IModuleHost* /*host*/) {
    return new M1::PodcastModule();
}
MCASTER1_PLUGIN_API void mcaster1_podcast_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
