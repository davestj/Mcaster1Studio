#include "AudioEngine.h"
#include "AudioDevice.h"
#include "AudioMixer.h"
#include "ModuleEvents.h"
#include <portaudio.h>
#include <QDebug>
#include <QTimer>
#include <cmath>
#include <algorithm>

namespace M1 {

// ─────────────────────────────────────────────────────────────────────────────
// PortAudio callback (real-time thread)
// ─────────────────────────────────────────────────────────────────────────────
int AudioEngine::paCallback(const void* inputBuffer,
                             void*       outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* /*timeInfo*/,
                             PaStreamCallbackFlags /*statusFlags*/,
                             void*       userData)
{
    auto* self = static_cast<AudioEngine*>(userData);

    // Build input AudioBuffer view
    AudioBuffer inBuf;
    inBuf.data       = const_cast<float*>(static_cast<const float*>(inputBuffer));
    inBuf.channels   = 2;  // assumed stereo; adjust per actual device config
    inBuf.frames     = static_cast<int>(framesPerBuffer);
    inBuf.sampleRate = self->m_sampleRate;
    inBuf.isValid    = (inputBuffer != nullptr);

    // Build output AudioBuffer view
    AudioBuffer outBuf;
    outBuf.data       = static_cast<float*>(outputBuffer);
    outBuf.channels   = 2;
    outBuf.frames     = static_cast<int>(framesPerBuffer);
    outBuf.sampleRate = self->m_sampleRate;
    outBuf.isValid    = (outputBuffer != nullptr);
    outBuf.silence();

    // Invoke registered callback (e.g. VUMeter reads levels here)
    // Use try_lock to avoid priority inversion; if locked, skip this block
    if (self->m_callbackMutex.try_lock()) {
        if (self->m_callback) {
            self->m_callback(inBuf, outBuf);
        }
        self->m_callbackMutex.unlock();
    }

    // Compute peak levels from input (for VU meters)
    if (inBuf.isValid && inBuf.frames > 0) {
        float peakL = 0.0f, peakR = 0.0f;
        for (int f = 0; f < inBuf.frames; ++f) {
            peakL = std::max(peakL, std::abs(inBuf.at(f, 0)));
            peakR = std::max(peakR, (inBuf.channels > 1) ?
                                    std::abs(inBuf.at(f, 1)) : peakL);
        }
        self->m_inLevelL.store(peakL, std::memory_order_relaxed);
        self->m_inLevelR.store(peakR, std::memory_order_relaxed);
    }

    if (outBuf.isValid && outBuf.frames > 0) {
        float peakL = 0.0f, peakR = 0.0f;
        for (int f = 0; f < outBuf.frames; ++f) {
            peakL = std::max(peakL, std::abs(outBuf.at(f, 0)));
            peakR = std::max(peakR, (outBuf.channels > 1) ?
                                    std::abs(outBuf.at(f, 1)) : peakL);
        }
        self->m_outLevelL.store(peakL, std::memory_order_relaxed);
        self->m_outLevelR.store(peakR, std::memory_order_relaxed);
    }

    return paContinue;
}

// ─────────────────────────────────────────────────────────────────────────────
// AudioEngine implementation
// ─────────────────────────────────────────────────────────────────────────────
AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    // Level update timer — fires on Qt thread every 50ms (20fps for VU meters)
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);
    connect(m_levelTimer, &QTimer::timeout, this, [this]() {
        const float il = m_inLevelL.load(std::memory_order_relaxed);
        const float ir = m_inLevelR.load(std::memory_order_relaxed);
        const float ol = m_outLevelL.load(std::memory_order_relaxed);
        const float or_ = m_outLevelR.load(std::memory_order_relaxed);
        emit levelsUpdated(il, ir, ol, or_);
        emit EventBus::instance().audioLevelsChanged(il, ir, ol, or_);
    });
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize() {
    if (m_state != AudioEngineState::Uninitialized) return true;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        m_lastError = QString("Pa_Initialize failed: %1").arg(Pa_GetErrorText(err));
        qCritical() << "[AudioEngine]" << m_lastError;
        setState(AudioEngineState::Error);
        return false;
    }

    qInfo() << "[AudioEngine] PortAudio initialized. Version:" << Pa_GetVersionText();
    qInfo() << "[AudioEngine] Host APIs:" << Pa_GetHostApiCount();

    // Log available host APIs
    for (int i = 0; i < Pa_GetHostApiCount(); ++i) {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
        if (api) qInfo() << "  -" << api->name << "(" << api->deviceCount << "devices)";
    }

    setState(AudioEngineState::Initialized);
    return true;
}

void AudioEngine::shutdown() {
    stopCueStream();
    stopStream();
    if (m_state != AudioEngineState::Uninitialized) {
        Pa_Terminate();
        setState(AudioEngineState::Uninitialized);
        qInfo() << "[AudioEngine] PortAudio terminated.";
    }
}

QList<AudioDeviceInfo> AudioEngine::inputDevices() const {
    return enumerateInputDevices();
}

QList<AudioDeviceInfo> AudioEngine::outputDevices() const {
    return enumerateOutputDevices();
}

AudioDeviceInfo AudioEngine::defaultInputDevice() const {
    return getDefaultInputDevice();
}

AudioDeviceInfo AudioEngine::defaultOutputDevice() const {
    return getDefaultOutputDevice();
}

bool AudioEngine::openStream(int inputDeviceIndex, int outputDeviceIndex,
                              int sampleRate, int framesPerBuffer) {
    if (m_state == AudioEngineState::Running) stopStream();
    if (m_state == AudioEngineState::Uninitialized) {
        if (!initialize()) return false;
    }

    m_sampleRate       = sampleRate;
    m_framesPerBuffer  = framesPerBuffer;
    m_inputDevIndex    = inputDeviceIndex;
    m_outputDevIndex   = outputDeviceIndex;

    PaStreamParameters inParams  = {};
    PaStreamParameters outParams = {};
    PaStreamParameters* pIn  = nullptr;
    PaStreamParameters* pOut = nullptr;

    if (inputDeviceIndex >= 0) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(inputDeviceIndex);
        inParams.device                    = inputDeviceIndex;
        inParams.channelCount              = std::min(2, d ? d->maxInputChannels : 2);
        inParams.sampleFormat              = paFloat32;
        inParams.suggestedLatency          = d ? d->defaultLowInputLatency : 0.01;
        inParams.hostApiSpecificStreamInfo = nullptr;
        pIn = &inParams;
    }

    if (outputDeviceIndex >= 0) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(outputDeviceIndex);
        outParams.device                    = outputDeviceIndex;
        outParams.channelCount              = std::min(2, d ? d->maxOutputChannels : 2);
        outParams.sampleFormat              = paFloat32;
        outParams.suggestedLatency          = d ? d->defaultLowOutputLatency : 0.01;
        outParams.hostApiSpecificStreamInfo = nullptr;
        pOut = &outParams;
    }

    PaError err = Pa_OpenStream(
        &m_stream,
        pIn, pOut,
        sampleRate,
        static_cast<unsigned long>(framesPerBuffer),
        paClipOff,
        &AudioEngine::paCallback,
        this
    );

    if (err != paNoError) {
        m_lastError = QString("Pa_OpenStream failed: %1").arg(Pa_GetErrorText(err));
        qCritical() << "[AudioEngine]" << m_lastError;
        setState(AudioEngineState::Error);
        return false;
    }

    qInfo() << "[AudioEngine] Stream opened."
            << "SR:" << sampleRate << "Frames:" << framesPerBuffer
            << "In:" << inputDeviceIndex << "Out:" << outputDeviceIndex;
    return true;
}

bool AudioEngine::startStream() {
    if (!m_stream) return false;
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        m_lastError = QString("Pa_StartStream failed: %1").arg(Pa_GetErrorText(err));
        qCritical() << "[AudioEngine]" << m_lastError;
        setState(AudioEngineState::Error);
        return false;
    }
    setState(AudioEngineState::Running);
    m_levelTimer->start();
    qInfo() << "[AudioEngine] Stream started.";
    return true;
}

void AudioEngine::stopStream() {
    m_levelTimer->stop();
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        setState(AudioEngineState::Initialized);
        qInfo() << "[AudioEngine] Stream stopped.";
    }
}

bool AudioEngine::isRunning() const {
    return m_stream && Pa_IsStreamActive(m_stream) == 1;
}

float AudioEngine::inputLevelL()  const { return m_inLevelL.load(std::memory_order_relaxed); }
float AudioEngine::inputLevelR()  const { return m_inLevelR.load(std::memory_order_relaxed); }
float AudioEngine::outputLevelL() const { return m_outLevelL.load(std::memory_order_relaxed); }
float AudioEngine::outputLevelR() const { return m_outLevelR.load(std::memory_order_relaxed); }

void AudioEngine::setAudioCallback(AudioCallback cb) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = std::move(cb);
}

AudioEngineState AudioEngine::state()           const { return m_state; }
QString          AudioEngine::lastError()        const { return m_lastError; }
int              AudioEngine::sampleRate()       const { return m_sampleRate; }
int              AudioEngine::framesPerBuffer()  const { return m_framesPerBuffer; }

void AudioEngine::setState(AudioEngineState s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
    emit EventBus::instance().audioEngineStateChanged(static_cast<int>(s));
}

// ─────────────────────────────────────────────────────────────────────────────
// CUE output stream (headphone monitor on separate device)
// ─────────────────────────────────────────────────────────────────────────────
int AudioEngine::paCueCallback(const void* /*inputBuffer*/,
                                void*       outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                PaStreamCallbackFlags /*statusFlags*/,
                                void*       userData)
{
    auto* self = static_cast<AudioEngine*>(userData);
    auto* out  = static_cast<float*>(outputBuffer);
    const int frames = static_cast<int>(framesPerBuffer);

    // Read from CUE ring buffer
    const int r     = self->m_cueRead.load(std::memory_order_relaxed);
    const int w     = self->m_cueWrite.load(std::memory_order_acquire);
    const int avail = w - r;
    const int cap   = kCueRingCapacity;
    const int mask  = cap - 1;
    const int toRead = std::min(avail, frames);

    for (int i = 0; i < toRead; ++i) {
        const int off = ((r + i) & mask) * 2;
        out[i * 2 + 0] = self->m_cueRing[off + 0];
        out[i * 2 + 1] = self->m_cueRing[off + 1];
    }
    // Zero remainder if ring didn't have enough
    for (int i = toRead; i < frames; ++i) {
        out[i * 2 + 0] = 0.0f;
        out[i * 2 + 1] = 0.0f;
    }
    self->m_cueRead.store(r + toRead, std::memory_order_release);
    return paContinue;
}

bool AudioEngine::openCueStream(int cueDeviceIndex) {
    stopCueStream();
    if (cueDeviceIndex < 0) return false;
    if (m_state == AudioEngineState::Uninitialized) {
        if (!initialize()) return false;
    }

    // Allocate ring buffer
    m_cueRing.resize(kCueRingCapacity * 2, 0.0f);
    m_cueWrite.store(0, std::memory_order_relaxed);
    m_cueRead.store(0,  std::memory_order_relaxed);

    const PaDeviceInfo* d = Pa_GetDeviceInfo(cueDeviceIndex);
    if (!d || d->maxOutputChannels < 1) {
        qWarning() << "[AudioEngine] CUE device" << cueDeviceIndex << "invalid.";
        return false;
    }

    PaStreamParameters outParams = {};
    outParams.device                    = cueDeviceIndex;
    outParams.channelCount              = std::min(2, d->maxOutputChannels);
    outParams.sampleFormat              = paFloat32;
    outParams.suggestedLatency          = d->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &m_cueStream,
        nullptr, &outParams,
        m_sampleRate,
        static_cast<unsigned long>(m_framesPerBuffer),
        paClipOff,
        &AudioEngine::paCueCallback,
        this
    );

    if (err != paNoError) {
        qWarning() << "[AudioEngine] CUE Pa_OpenStream failed:" << Pa_GetErrorText(err);
        m_cueStream = nullptr;
        return false;
    }

    err = Pa_StartStream(m_cueStream);
    if (err != paNoError) {
        qWarning() << "[AudioEngine] CUE Pa_StartStream failed:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_cueStream);
        m_cueStream = nullptr;
        return false;
    }

    m_cueDevIndex = cueDeviceIndex;
    qInfo() << "[AudioEngine] CUE stream opened on device" << cueDeviceIndex
            << d->name;
    return true;
}

void AudioEngine::stopCueStream() {
    if (m_cueStream) {
        Pa_StopStream(m_cueStream);
        Pa_CloseStream(m_cueStream);
        m_cueStream = nullptr;
        qInfo() << "[AudioEngine] CUE stream stopped.";
    }
}

bool AudioEngine::isCueRunning() const {
    return m_cueStream && Pa_IsStreamActive(m_cueStream) == 1;
}

void AudioEngine::writeCue(const float* data, int frames) {
    if (m_cueRing.empty()) return;
    const int w    = m_cueWrite.load(std::memory_order_relaxed);
    const int r    = m_cueRead.load(std::memory_order_acquire);
    const int free = kCueRingCapacity - (w - r);
    const int cap  = kCueRingCapacity;
    const int mask = cap - 1;
    const int toWrite = std::min(free, frames);

    for (int i = 0; i < toWrite; ++i) {
        const int off = ((w + i) & mask) * 2;
        m_cueRing[off + 0] = data[i * 2 + 0];
        m_cueRing[off + 1] = data[i * 2 + 1];
    }
    m_cueWrite.store(w + toWrite, std::memory_order_release);
}

} // namespace M1
